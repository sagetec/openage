// movement/unit_movement.h
// Standalone illustration of how the OpenAge Move system uses the pathfinder.
// This is a cleaned-up, engine-agnostic version of gamestate/system/move.cpp.
//
// In OpenAge the actual Move system uses:
//   - nyan (a data-definition language) for speed/turn-speed lookup
//   - curve-based position storage (not plain coordinates)
//   - the EventLoop for timing
//
// This file strips all of that and shows the pure pathfinding integration pattern.

#pragma once

#include <memory>
#include <vector>

// ── Include from standalone_pathfinding ───────────────────────────────────────
// Adjust these relative paths if your include setup differs.
#include "../pathfinding/path.h"
#include "../pathfinding/pathfinder.h"
#include "../pathfinding/types.h"

namespace standalone {

// ---------------------------------------------------------------------------
// Simple 2D world position (replace with your game's position type).
// In openage this is coord::phys3 (a FixedPoint 3-axis physical coordinate).
// ---------------------------------------------------------------------------
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// ---------------------------------------------------------------------------
// PathResult: a list of waypoints from start to destination.
// The first waypoint is always the start. The last is always the destination.
// ---------------------------------------------------------------------------
struct MovementPath {
    std::vector<Vec2> waypoints; ///< Ordered list of world-space positions
    bool found = false;          ///< Whether a valid path was found
};

// ---------------------------------------------------------------------------
// UnitMovement
//
// Shows the integration pattern between a unit and the pathfinder.
//
// In OpenAge:
//   map->get_pathfinder()    returns the shared_ptr<path::Pathfinder>
//   move_ability.path_type   resolves to a grid_id_t
//   pos_component            stores keyframes (positions over time)
//
// Here we replace all of that with bare data structures.
// ---------------------------------------------------------------------------
class UnitMovement {
public:
    // ── Configuration ─────────────────────────────────────────────────────
    double move_speed = 5.0;   ///< World units per second
    double turn_speed = 180.0; ///< Degrees per second (set to 0 for instant turn)

    // ── State ─────────────────────────────────────────────────────────────
    Vec2   position;           ///< Current world-space position
    double angle = 0.0;        ///< Current facing angle (degrees)

    // ── Path request helper ───────────────────────────────────────────────
    /**
     * Request a path from the pathfinder and return a MovementPath.
     *
     * @param pathfinder    Shared pathfinder instance (owns all grids).
     * @param grid_id       Which movement grid to search (e.g. ground vs. air).
     * @param start         Start position in world space.
     * @param destination   Target position in world space.
     * @param request_time  Time of request (used for cache invalidation).
     *
     * @return MovementPath with ordered waypoints, or {found=false} if no path.
     */
    static MovementPath find_path(
        const std::shared_ptr<openage::path::Pathfinder> &pathfinder,
        openage::path::grid_id_t                          grid_id,
        const Vec2                                        &start,
        const Vec2                                        &destination,
        const openage::time::time_t                       &request_time
    ) {
        // Convert world-space positions to tile coordinates.
        // In openage this is phys3.to_tile() — adjust the scale factor
        // to match your tile size (e.g. 1 world unit = 1 tile).
        openage::coord::tile start_tile{
            static_cast<openage::coord::tile_t>(start.x),
            static_cast<openage::coord::tile_t>(start.y)
        };
        openage::coord::tile end_tile{
            static_cast<openage::coord::tile_t>(destination.x),
            static_cast<openage::coord::tile_t>(destination.y)
        };

        // Build and submit the pathfinding request.
        openage::path::PathRequest request{
            grid_id,
            start_tile,
            end_tile,
            request_time,
        };

        auto tile_path = pathfinder->get_path(request);

        if (tile_path.status != openage::path::PathResult::FOUND) {
            return {.found = false};
        }

        // Convert tile waypoints back to world-space.
        // In openage, tile.to_phys3_center() returns the physical center of the tile.
        std::vector<Vec2> waypoints;
        waypoints.reserve(tile_path.waypoints.size());
        waypoints.push_back(start);  // first point is exact start position

        // Skip first (start tile) and last (end tile) — replace with exact coords.
        for (size_t i = 1; i < tile_path.waypoints.size() - 1; ++i) {
            const auto &wp = tile_path.waypoints[i];
            // Center of tile: add 0.5 to convert tile-index to world center.
            waypoints.push_back({
                static_cast<double>(wp.ne) + 0.5,
                static_cast<double>(wp.se) + 0.5
            });
        }
        waypoints.push_back(destination);  // last point is exact destination

        return {.waypoints = std::move(waypoints), .found = true};
    }

    /**
     * Compute the total time (in seconds) to travel the given path.
     *
     * This implements the same logic as openage's Move::move_default(),
     * accounting for turn time at each waypoint direction change.
     *
     * @param path The MovementPath from find_path().
     * @return Total travel time in seconds.
     */
    double compute_travel_time(const MovementPath &path) const {
        if (!path.found || path.waypoints.size() < 2) {
            return 0.0;
        }

        double total_time = 0.0;
        double current_angle = this->angle;

        for (size_t i = 1; i < path.waypoints.size(); ++i) {
            const Vec2 &prev = path.waypoints[i - 1];
            const Vec2 &cur  = path.waypoints[i];

            // Direction vector
            double dx = cur.x - prev.x;
            double dy = cur.y - prev.y;
            double segment_length = std::sqrt(dx * dx + dy * dy);

            // Target angle for this segment (degrees)
            double target_angle = std::atan2(dy, dx) * (180.0 / M_PI);
            if (target_angle < 0) target_angle += 360.0;

            // Turn time (if turn speed is finite)
            if (turn_speed > 0.0) {
                double angle_diff = std::abs(target_angle - current_angle);
                if (angle_diff > 180.0) angle_diff = 360.0 - angle_diff;
                total_time += angle_diff / turn_speed;
                current_angle = target_angle;
            }

            // Movement time
            if (move_speed > 0.0) {
                total_time += segment_length / move_speed;
            }
        }

        return total_time;
    }
};

} // namespace standalone
