// main_demo.cpp
//
// End-to-end demonstration of the standalone pathfinding + movement library.
//
// WHAT IT DOES
// ────────────
//  1. Builds a 4×4 sector grid (each sector = 10×10 tiles → 40×40 tile world).
//  2. Places a wall of impassable cells down the middle columns.
//  3. Leaves a gap in the wall so a path exists.
//  4. Runs the pathfinder from top-left to bottom-right.
//  5. Prints the tile waypoints and the computed travel time.
//  6. Iterates over the path tick-by-tick to simulate unit movement.
//
// BUILDING
// ────────
//   mkdir build && cd build
//   cmake ..
//   cmake --build .
//   ./pathfinding_demo          (Linux/macOS)
//   .\Debug\pathfinding_demo.exe  (Windows MSVC)

#include <cstdio>
#include <memory>

// ── Standalone library headers ────────────────────────────────────────────────
#include "movement/path_map.h"
#include "movement/move_system.h"
#include "pathfinding/definitions.h"
#include "pathfinding/types.h"
#include "time/time.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void print_separator() {
    std::puts("──────────────────────────────────────────────────────────────");
}

static void print_path(const standalone::MovementPath &path) {
    if (!path.found) {
        std::puts("  [NO PATH FOUND]");
        return;
    }
    std::printf("  Waypoints (%zu):\n", path.waypoints.size());
    for (size_t i = 0; i < path.waypoints.size(); ++i) {
        std::printf("    [%2zu]  (%.1f, %.1f)\n",
                    i, path.waypoints[i].x, path.waypoints[i].y);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Map layout
// ─────────────────────────────────────────────────────────────────────────────
//
//  Grid:  4 × 4 sectors
//  Each sector:  10 × 10 tiles
//  Total world:  40 × 40 tiles
//
//  Wall: columns 18 and 19 of every row, EXCEPT rows 15–19 (a gap at the bottom
//        half), so there's exactly one corridor to route through.
//
//  Visually (X = impassable, . = passable, G = gap):
//
//   col: 0         18 19   39
//        . . . . . X  X  . . .   row 0
//        . . . . . X  X  . . .   ...
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . X  X  . . .
//        . . . . . G  G  . . .   row 15  ← gap starts
//        . . . . . G  G  . . .
//        . . . . . G  G  . . .
//        . . . . . G  G  . . .
//        . . . . . G  G  . . .   row 19
//        . . . . . X  X  . . .   row 20  ← wall resumes
//        ...
//        . . . . . X  X  . . .   row 39

static constexpr size_t SECTOR_SIZE = 10;  // tiles per sector side
static constexpr size_t GRID_W = 4;        // sectors wide
static constexpr size_t GRID_H = 4;        // sectors tall
static constexpr size_t WORLD_W = GRID_W * SECTOR_SIZE;  // 40 tiles
static constexpr size_t WORLD_H = GRID_H * SECTOR_SIZE;  // 40 tiles

// Return impassable cost if this world-tile should be a wall.
static openage::path::cost_t tile_cost(size_t world_col, size_t world_row) {
    using namespace openage::path;
    bool is_wall_col = (world_col == 18 || world_col == 19);
    bool in_gap      = (world_row >= 15 && world_row <= 19);
    if (is_wall_col && !in_gap)
        return COST_IMPASSABLE;
    return COST_MIN; // passable, minimum cost
}

// Convert a world-tile position to (sector_index, tile_index_within_sector).
static std::pair<size_t, size_t> world_to_sector_tile(size_t col, size_t row) {
    size_t sec_x   = col / SECTOR_SIZE;
    size_t sec_y   = row / SECTOR_SIZE;
    size_t sec_idx = sec_y * GRID_W + sec_x;
    size_t loc_x   = col % SECTOR_SIZE;
    size_t loc_y   = row % SECTOR_SIZE;
    size_t tile_idx = loc_y * SECTOR_SIZE + loc_x;
    return {sec_idx, tile_idx};
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    print_separator();
    std::puts(" Standalone Flow-Field Pathfinding + Movement Demo");
    print_separator();

    // ── 1. Create the map and add one movement grid ───────────────────────────
    standalone::PathMap map;
    auto grid_id = map.add_named_grid("ground",
                                      openage::util::Vector2s{GRID_W, GRID_H},
                                      SECTOR_SIZE);
    std::printf("Grid created: id=%zu, %zu×%zu sectors, %zu tiles/sector side\n",
                grid_id, GRID_W, GRID_H, SECTOR_SIZE);

    // ── 2. Set tile costs ─────────────────────────────────────────────────────
    size_t impassable_count = 0;
    for (size_t row = 0; row < WORLD_H; ++row) {
        for (size_t col = 0; col < WORLD_W; ++col) {
            auto cost = tile_cost(col, row);
            auto [sec_idx, tile_idx] = world_to_sector_tile(col, row);
            map.set_cost(grid_id, sec_idx, tile_idx, cost);
            if (cost == openage::path::COST_IMPASSABLE)
                ++impassable_count;
        }
    }
    std::printf("Costs set:    %zu impassable tiles, %zu passable tiles\n",
                impassable_count, WORLD_W * WORLD_H - impassable_count);

    // ── 3. Build portals ──────────────────────────────────────────────────────
    map.build_portals();
    std::puts("Portals built between adjacent sectors.");

    // ── 4. Define start / destination ─────────────────────────────────────────
    standalone::Vec2 start       = {1.5,  1.5};   // top-left area
    standalone::Vec2 destination = {38.5, 38.5};  // bottom-right area

    std::printf("\nStart:       (%.1f, %.1f)  →  tile (%lld, %lld)\n",
                start.x, start.y,
                (long long)standalone::MoveSystem::world_to_tile(start).ne,
                (long long)standalone::MoveSystem::world_to_tile(start).se);
    std::printf("Destination: (%.1f, %.1f)  →  tile (%lld, %lld)\n",
                destination.x, destination.y,
                (long long)standalone::MoveSystem::world_to_tile(destination).ne,
                (long long)standalone::MoveSystem::world_to_tile(destination).se);

    // ── 5. Run the pathfinder ─────────────────────────────────────────────────
    print_separator();
    std::puts(" Pathfinding...");

    auto &pathfinder = map.get_pathfinder();
    auto  time       = openage::time::TIME_ZERO;
    auto  path       = standalone::MoveSystem::find_path(
                            pathfinder, grid_id, start, destination, time);

    if (!path.found) {
        std::puts("ERROR: No path found! Check map layout.");
        return 1;
    }
    std::printf("Path FOUND!  %zu waypoints.\n", path.waypoints.size());
    print_path(path);

    // ── 6. Travel time ────────────────────────────────────────────────────────
    print_separator();
    standalone::UnitState unit;
    unit.position   = start;
    unit.move_speed = 5.0;   // 5 world‑units / second
    unit.turn_speed = 90.0;  // 90 degrees / second

    double travel_secs = standalone::MoveSystem::compute_travel_time(unit, path);
    std::printf(" Unit stats:   speed=%.1f u/s   turn=%.1f deg/s\n",
                unit.move_speed, unit.turn_speed);
    std::printf(" Travel time:  %.2f seconds\n", travel_secs);

    // ── 7. Simulate unit movement along waypoints ─────────────────────────────
    print_separator();
    std::puts(" Simulation (tick-by-tick, dt = 0.5 s):");
    std::puts("   tick |  position       | segment");

    constexpr double DT = 0.5;   // seconds per tick
    standalone::Vec2 pos = start;
    size_t wp_idx = 1;           // next waypoint to move toward

    for (int tick = 0; tick <= 200 && wp_idx < path.waypoints.size(); ++tick) {
        const standalone::Vec2 &target_wp = path.waypoints[wp_idx];
        standalone::Vec2 dir = (target_wp - pos).normalized();
        double dist_to_wp    = (target_wp - pos).length();
        double step          = unit.move_speed * DT;

        if (step >= dist_to_wp) {
            pos = target_wp;
            ++wp_idx;
        }
        else {
            pos = pos + dir * step;
        }

        if (tick % 5 == 0 || wp_idx >= path.waypoints.size()) {
            std::printf("   %4d |  (%.2f, %.2f)  | → wp %zu\n",
                        tick, pos.x, pos.y, wp_idx);
        }
    }

    print_separator();
    std::puts(" Demo complete.");
    return 0;
}
