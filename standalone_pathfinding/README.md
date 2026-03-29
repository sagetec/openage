# standalone_pathfinding

A self-contained C++ library that extracts the **hierarchical flow-field pathfinder** and **unit movement/avoidance system** from the [OpenAge](https://github.com/SFTtech/openage) real-time strategy engine.

Drop it into any C++ 20 project — no OpenAge build system, no nyan, no Python, no Qt.

---

## What's included

| Directory | What it contains |
|-----------|-----------------|
| `pathfinding/` | Core flow-field pathfinder (copied verbatim from OpenAge) |
| `movement/` | Standalone adapter layer: `PathMap`, `MoveSystem` |
| `datastructure/` | `pairing_heap.h` — A* open-list (header-only, copied from OpenAge) |
| `util/` | `fixed_point.h`, `hash`, `misc`, `vector`, `compiler` (copied from OpenAge) |
| `time/` | `time.h` — `time::time_t` typedef (FixedPoint wrapper) |
| `stubs/` | Shim headers replacing OpenAge's code-generated `coord/` and `error/log/` systems |
| `pathfinding_coords.h` | Hand-written replacement for the code-generated coordinate system |
| `main_demo.cpp` | End-to-end working demo |
| `CMakeLists.txt` | Build system |

---

## How the pathfinder works

This is a **hierarchical flow-field** pathfinder — the same technique used in
Supreme Commander and Planetary Annihilation. It runs in two stages:

### Stage 1 — High-level A\* over portals

The grid is divided into **sectors** (e.g. 10×10 tile chunks).  Adjacent sectors
are connected by **portals** (shared boundary cells where movement is possible).
A* searches the portal graph to find which sectors to traverse.

### Stage 2 — Flow field integration per sector

For each sector on the route the **Integrator** runs a Dijkstra wave backward
from the goal, producing an **IntegrationField** (cost-to-goal per cell).
The **FlowField** is then built from that: each cell stores the cheapest
neighbouring direction encoded in 4 bits ( `flow_dir_t`: N/NE/E/SE/S/SW/W/NW ).

### Unit movement (avoidance)

**OpenAge does not use boids, RVO, or any steering algorithm.**  
Avoidance is built into the flow field:

1. All units heading to the **same destination** share **one flow field** — no
   per-unit path computation.
2. Impassable cells (`cost = 255`) are never relaxed by the Dijkstra wave, so
   the integrated cost naturally encodes detours around obstacles.
3. The **Line-Of-Sight (LOS) pass** runs before integration: cells with an
   unobstructed straight line to the goal are flagged `FLOW_LOS_MASK`.
   Units in these cells skip the grid and move directly to the destination.
4. Each simulation tick a unit reads its current cell's `flow_t` value and
   moves in the encoded direction × `move_speed` × `dt`.

```
frame tick:
    tile_idx   = world_to_tile(unit.pos)
    flow_val   = flow_field->get_flow(tile_idx)
    if flow_val & FLOW_LOS_MASK:
        direction = normalize(destination - unit.pos)
    else:
        direction = FLOW_DIR_VECTORS[flow_val & FLOW_DIR_MASK]
    unit.pos += direction * move_speed * dt
```

---

## Key classes

### `PathMap`  (`movement/path_map.h`)

Owns the `Pathfinder` and one or more movement grids.  
Replaces `openage::gamestate::Map`.

```cpp
standalone::PathMap map;

// Add a ground-movement grid: 4×4 sectors, each 10×10 tiles
auto grid_id = map.add_named_grid("ground", {4, 4}, 10);

// Fill in tile costs (1 = free, 255 = impassable)
map.set_cost(grid_id, sector_index, tile_index, openage::path::COST_MIN);
map.set_cost(grid_id, sector_index, tile_index, openage::path::COST_IMPASSABLE);

// Build portals (call once after all costs are set)
map.build_portals();

auto &pathfinder = map.get_pathfinder();
```

### `MoveSystem`  (`movement/move_system.h`)

All-static helper class.  Replaces `openage::gamestate::system::Move`.

```cpp
// Request a path
auto path = standalone::MoveSystem::find_path(
    pathfinder, grid_id, start_pos, dest_pos, time);

// Compute travel time with turn delays
standalone::UnitState unit{ .move_speed = 5.0, .turn_speed = 90.0 };
double secs = standalone::MoveSystem::compute_travel_time(unit, path);

// Per-frame direction from flow field (avoidance)
Vec2 dir = standalone::MoveSystem::get_flow_direction(
    flow_field, grid, unit.position, destination);
unit.position = unit.position + dir * unit.move_speed * dt;
```

### `Pathfinder`  (`pathfinding/pathfinder.h`)

The top-level engine class.  You normally interact with it through `PathMap`,
but you can use it directly:

```cpp
auto pf = std::make_shared<openage::path::Pathfinder>();
pf->add_grid(grid);
auto path = pf->get_path(request);   // returns openage::path::Path
```

---

## Cost field encoding

| Value | Meaning |
|-------|---------|
| `0`   | Uninitialised (never use) |
| `1–254` | Passable (higher = slower/more expensive) |
| `255` | Impassable (`COST_IMPASSABLE`) |

---

## The coordinate system (`pathfinding_coords.h`)

OpenAge uses an **isometric NE/SE axis** coordinate system generated at build
time from a template.  We replace the generated files with a single
hand-written header:

| Type | Axes | Equivalent |
|------|------|------------|
| `coord::tile` | `.ne`, `.se` (`int64_t`) | absolute cell position |
| `coord::tile_delta` | `.ne`, `.se` (`int64_t`) | relative offset |
| `coord::chunk` | `.ne`, `.se` (`int32_t`) | sector position |

For a standard **axis-aligned** (non-isometric) grid, treat `.ne` as `column`
and `.se` as `row`.

---

## Building

```bash
# Prerequisites: CMake ≥ 3.16, a C++20 compiler (GCC 11+, Clang 13+, MSVC 19.29+)

mkdir build && cd build
cmake ..                    # configure
cmake --build .             # compile
./pathfinding_demo          # run demo (Linux/macOS)
.\Debug\pathfinding_demo.exe  # run demo (Windows)
```

The CMake build produces three targets:

| Target | Type | Contents |
|--------|------|----------|
| `pathfinding` | static lib | core flow-field engine |
| `pathfinding_legacy` | static lib | old A* (optional) |
| `movement` | static lib | PathMap + MoveSystem |
| `pathfinding_demo` | executable | end-to-end demo |

To link against `pathfinding` + `movement` from your own CMake project:

```cmake
add_subdirectory(standalone_pathfinding)
target_link_libraries(my_game PRIVATE pathfinding movement)
```

---

## Integrating into your project

1. Copy this folder into your project.
2. Add via `add_subdirectory` (see above), **or** compile the `.cpp` files
   directly and add `standalone_pathfinding/` + `standalone_pathfinding/stubs/`
   to your include path.
3. Use `PathMap` to build your world's cost grid.
4. Use `MoveSystem::find_path()` to request paths.
5. Each game tick, call `MoveSystem::get_flow_direction()` with the cached
   `FlowField` to update unit positions.

### Replacing `standalone::Vec2`

`Vec2` is a thin placeholder type in `movement/move_system.h`.  Search-replace
it with your engine's vector type and adjust `TILE_SIZE` to match your tile size
in world units.

---

## Files NOT included (intentionally omitted)

| Omitted | Why |
|---------|-----|
| `pathfinding/demo/` | Depends on OpenAge renderer |
| `coord/` (full) | Replaced by `pathfinding_coords.h` + stubs |
| `error/`, `log/` (full) | Replaced by `stubs/error/error.h` + `stubs/log/log.h` |
| `libopenage/gamestate/` | Full game-state system (nyan, curves, ECS) |
| `nyan` | Data-definition language, not needed for pathfinding |

---

## Original source

- Repository: https://github.com/SFTtech/openage
- Pathfinding source: `libopenage/pathfinding/`
- Movement system: `libopenage/gamestate/system/move.cpp`
- Map integration: `libopenage/gamestate/map.cpp`
