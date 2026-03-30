// movement/move_system.h
// Standalone unit movement & avoidance system.
//
// This is the engine-agnostic equivalent of:
//   openage/libopenage/gamestate/system/move.cpp  (Move system)
//
// In OpenAge the Move system:
//   1. Reads speed/turn_speed from a nyan ability object.
//   2. Stores positional keyframes in a curve-based position component.
//   3. Uses coord::phys3 for world-space and coord::tile for grid cells.
//   4. Pulls the Pathfinder out of Map->get_pathfinder().
//
// Here we replace all of that with plain C++ structs — zero engine deps.
//
// ─────────────────────────────────────────────────────────────────────────────
// HOW THE AVOIDANCE SYSTEM WORKS (Flow-Field)
// ─────────────────────────────────────────────────────────────────────────────
//
//  OpenAge does NOT use traditional steering (no boids, no RVO).
//  Avoidance is baked into the flow field itself:
//
//  1. All units moving to the SAME destination share ONE flow field.
//     → No per-unit path search needed.
//
//  2. During cost integration (Dijkstra wave from target → outward):
//     • Impassable cells (cost=255) are never relaxed.
//     • High-cost cells slow the wave, so the resulting integrated cost
//       already encodes "prefer cheap paths around obstacles".
//
//  3. The flow field stores a direction per cell (N/NE/E/SE/S/SW/W/NW).
//     • A unit reads its current cell's direction and moves that way.
//     • Cells flagged FLOW_LOS_MASK have line-of-sight to the goal:
//       those units skip the grid and move straight to the destination.
//
//  4. At runtime, units sample the field each frame:
//       flow_t dir = flow_field->get_flow(tile_index);
//       move unit by DIR_VECTORS[dir & FLOW_DIR_MASK] * speed * dt
//     This is what get_flow_direction_world() below illustrates.
//
//  5. Dynamic obstacles are handled by marking affected cost-field cells
//     impassable then calling Integrator::integrate() again for that sector.
//     The field cache automatically re-uses unchanged sectors.
//
// ─────────────────────────────────────────────────────────────────────────────

#pragma once

#include <cmath>
#include <memory>
#include <vector>

#include "../pathfinding/definitions.h"
#include "../pathfinding/flow_field.h"
#include "../pathfinding/path.h"
#include "../pathfinding/pathfinder.h"
#include "../pathfinding/sector.h"
#include "../pathfinding/grid.h"
#include "../pathfinding/types.h"
#include "../pathfinding_coords.h"
#include "../time/time.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace standalone {

// ── World-space 2-D position ──────────────────────────────────────────────────
// Replace with your game's Vec2 type (glm::vec2, sf::Vector2f, etc.)
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    Vec2 operator+(const Vec2 &o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2 &o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s)      const { return {x * s,   y * s};   }

    double length() const { return std::sqrt(x * x + y * y); }

    Vec2 normalized() const {
        double len = length();
        if (len < 1e-9) return {0.0, 0.0};
        return {x / len, y / len};
    }
};

// ── Result of a path request ──────────────────────────────────────────────────
struct MovementPath {
    std::vector<Vec2> waypoints; ///< Ordered: start → ... → destination
    bool found = false;          ///< False if no route exists
};

// ── Per-unit state ────────────────────────────────────────────────────────────
struct UnitState {
    Vec2   position;
    double angle      = 0.0;   ///< Current heading in degrees [0, 360)
    double move_speed = 5.0;   ///< World-units per second
    double turn_speed = 180.0; ///< Degrees per second; 0 = instant turn
};

// ─────────────────────────────────────────────────────────────────────────────
// MoveSystem
//
// Drop-in equivalent of openage::gamestate::system::Move.
// All methods are static — no state, no dependencies.
// ─────────────────────────────────────────────────────────────────────────────
class MoveSystem {
public:
    // ── Tile ↔ world conversion ─────────────────────────────────────────────
    // Default: 1 tile = 1 world unit.
    static constexpr double TILE_SIZE = 1.0;

    /**
     * World position → tile coordinate.
     * Equivalent to openage coord::phys3::to_tile().
     */
    static openage::coord::tile world_to_tile(const Vec2 &pos) {
        return {
            static_cast<openage::coord::tile_t>(std::floor(pos.x / TILE_SIZE)),
            static_cast<openage::coord::tile_t>(std::floor(pos.y / TILE_SIZE))
        };
    }

    /**
     * Tile → center of that tile in world space.
     * Equivalent to openage coord::tile::to_phys3_center().
     */
    static Vec2 tile_center(const openage::coord::tile &t) {
        return {
            (static_cast<double>(t.ne) + 0.5) * TILE_SIZE,
            (static_cast<double>(t.se) + 0.5) * TILE_SIZE
        };
    }

    // ── Path request ────────────────────────────────────────────────────────

    /**
     * Request a path from the pathfinder.
     *
     * Equivalent to openage::gamestate::system::find_path().
     *
     * @param pathfinder   Shared pathfinder (owns all grids + integrator cache).
     * @param grid_id      Which movement grid to search (ground, air, water…).
     * @param start        World-space start position.
     * @param destination  World-space destination.
     * @param time         Simulation time of the request (for field-cache invalidation).
     *
     * @return MovementPath with ordered waypoints, or {found=false} if blocked.
     */
    static MovementPath find_path(
        const std::shared_ptr<openage::path::Pathfinder> &pathfinder,
        openage::path::grid_id_t                          grid_id,
        const Vec2                                       &start,
        const Vec2                                       &destination,
        const openage::time::time_t                      &time)
    {
        openage::path::PathRequest request{
            grid_id,
            world_to_tile(start),
            world_to_tile(destination),
            time,
        };

        auto tile_path = pathfinder->get_path(request);

        if (tile_path.status != openage::path::PathResult::FOUND) {
            return {.found = false};
        }

        // Convert tile waypoints → world positions.
        // • First  waypoint = exact start   (not tile centre).
        // • Middle waypoints = tile_center() for intermediate tiles.
        // • Last   waypoint = exact destination.
        std::vector<Vec2> wps;
        wps.reserve(tile_path.waypoints.size());
        wps.push_back(start);

        const auto &tw = tile_path.waypoints;
        for (size_t i = 1; i + 1 < tw.size(); ++i) {
            wps.push_back(tile_center(tw[i]));
        }
        wps.push_back(destination);

        return {.waypoints = std::move(wps), .found = true};
    }

    // ── Travel-time calculation ─────────────────────────────────────────────

    /**
     * Compute total travel time (seconds) for `unit` to walk `path`.
     *
     * Mirrors the per-waypoint loop in openage's Move::move_default():
     *   for each segment → apply rotation time → apply movement time.
     *
     * @return Total seconds, or 0.0 if path is empty / not found.
     */
    static double compute_travel_time(const UnitState    &unit,
                                      const MovementPath &path)
    {
        if (!path.found || path.waypoints.size() < 2)
            return 0.0;

        double total   = 0.0;
        double heading = unit.angle;

        for (size_t i = 1; i < path.waypoints.size(); ++i) {
            const Vec2 seg    = path.waypoints[i] - path.waypoints[i - 1];
            const double len  = seg.length();
            double target_deg = std::atan2(seg.y, seg.x) * (180.0 / M_PI);
            if (target_deg < 0.0) target_deg += 360.0;

            // Rotation time (shortest arc)
            if (unit.turn_speed > 0.0) {
                double diff = std::abs(target_deg - heading);
                if (diff > 180.0) diff = 360.0 - diff;
                total  += diff / unit.turn_speed;
                heading = target_deg;
            }

            // Movement time
            if (unit.move_speed > 0.0)
                total += len / unit.move_speed;
        }

        return total;
    }

    // ── Per-frame flow-field direction query ────────────────────────────────
    //
    // This is the runtime avoidance step: every frame, read the direction
    // from the pre-computed flow field for the unit's current cell and move
    // in that direction.  All units sharing the same destination & grid share
    // the same field — no per-unit re-computation is needed.

    /**
     * @brief Flow-field direction vectors, indexed by flow_dir_t.
     *
     * The openage coordinate system uses NE/SE axes (isometric), but here we
     * expose them as simple (x, y) screen-space vectors for portability.
     * Adjust if your renderer uses a different axis convention.
     *
     *  Index: 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
     */
    static constexpr Vec2 FLOW_DIR_VECTORS[8] = {
        { 0.0, -1.0},  // NORTH
        { 1.0, -1.0},  // NORTH_EAST
        { 1.0,  0.0},  // EAST
        { 1.0,  1.0},  // SOUTH_EAST
        { 0.0,  1.0},  // SOUTH
        {-1.0,  1.0},  // SOUTH_WEST
        {-1.0,  0.0},  // WEST
        {-1.0, -1.0},  // NORTH_WEST
    };

    /**
     * Query the flow field for the preferred movement direction of a unit.
     *
     * Call this each simulation tick to get the normalised movement vector.
     * The flow field for a sector is obtained externally (e.g. via Integrator)
     * and passed in — the Pathfinder's integrator is private, so callers must
     * cache their flow fields after calling pathfinder->get_path().
     *
     * In practice the Pathfinder already stores computed flow fields internally
     * for the duration of the simulation step.  This helper shows the decode
     * logic you'd use if you retained a pointer to a FlowField.
     *
     * @param flow_field   The FlowField for the sector containing unit_pos.
     * @param grid         The grid the unit is on (to get sector_size).
     * @param unit_pos     Current world position of the unit.
     * @param destination  Path destination (for LOS cells: move straight there).
     *
     * @return Normalised Vec2 the unit should move toward this frame.
     *         Returns {0,0} if the cell has no direction (e.g. at destination).
     */
    static Vec2 get_flow_direction(
        const std::shared_ptr<openage::path::FlowField> &flow_field,
        const std::shared_ptr<openage::path::Grid>      &grid,
        const Vec2                                      &unit_pos,
        const Vec2                                      &destination)
    {
        using namespace openage::path;

        // Tile and local index within the sector
        auto tile        = world_to_tile(unit_pos);
        size_t ss        = grid->get_sector_size();
        size_t local_ne  = static_cast<size_t>(tile.ne) % ss;
        size_t local_se  = static_cast<size_t>(tile.se) % ss;
        size_t tile_idx  = local_se * ss + local_ne;

        flow_t flow_val  = flow_field->get_cell(tile_idx);

        // LOS cells: straight line to goal
        if (flow_val & FLOW_LOS_MASK) {
            return (destination - unit_pos).normalized();
        }

        // No direction encoded (impassable or uninitialised)
        if (!(flow_val & FLOW_PATHABLE_MASK)) {
            return {0.0, 0.0};
        }

        uint8_t dir = static_cast<uint8_t>(flow_val & FLOW_DIR_MASK);
        if (dir < 8) {
            return FLOW_DIR_VECTORS[dir].normalized();
        }

        return {0.0, 0.0};
    }
};

} // namespace standalone
