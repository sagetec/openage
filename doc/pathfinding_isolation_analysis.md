# OpenAge Pathfinding – Isolation Analysis

## What It Is

This is a **hierarchical flow-field pathfinder** — the same technique used in large-scale RTS games (e.g. Supreme Commander, Planetary Annihilation). It works in two phases:

1. **High-level A\***: finds the route through sector *portals* (doorways between grid sectors).
2. **Flow-field integration**: for each sector on that route, computes a vector field so units naturally flow toward the target, without requiring per-unit path searches.

---

## Files to Extract

### Core Pathfinding (`libopenage/pathfinding/`)

| File | Role |
|------|------|
| `types.h` / `types.cpp` | All fundamental typedefs (`cost_t`, `flow_t`, `sector_id_t`, etc.) and enums |
| `definitions.h` / `definitions.cpp` | Compile-time constants (impassable cost, LOS masks, etc.) |
| `cost_field.h` / `cost_field.cpp` | Input cost grid per sector (uint8 per cell) |
| `integration_field.h` / `integration_field.cpp` | Dijkstra-wave integration producing per-cell `integrated_t` values |
| `flow_field.h` / `flow_field.cpp` | Builds directional flow vectors from integration field |
| `field_cache.h` / `field_cache.cpp` | Cache for reusing already-computed integration/flow field pairs |
| `integrator.h` / `integrator.cpp` | Orchestrates integration + flow field building; owns the cache |
| `portal.h` / `portal.cpp` | Bidirectional door between two sectors with LOS-connected exits |
| `sector.h` / `sector.cpp` | One tile-grid sector with a cost field and portal list |
| `grid.h` / `grid.cpp` | Collection of sectors; initializes portals between adjacent sectors |
| `path.h` / `path.cpp` | `PathRequest` and `Path` structs (simple data containers) |
| `pathfinder.h` / `pathfinder.cpp` | Top-level class; runs A* over portals then gets waypoints from flow fields |
| `tests.cpp` | Unit tests — optional |

### Legacy (separate, older A* implementation — optional)

| File | Role |
|------|------|
| `legacy/a_star.h+cpp` | Simple A* (not flow-field) |
| `legacy/heuristics.h+cpp` | Manhattan/Chebyshev heuristics for the legacy A* |
| `legacy/path.h+cpp` | Path data structure for the legacy system |

---

## External Dependencies

### 1. `coord/` — Coordinate System (Main Obstacle)

| File needed | Why |
|------------|-----|
| `coord/tile.h+cpp` | `coord::tile` and `coord::tile_delta`. Used **everywhere** for cell positions. |
| `coord/chunk.h+cpp` | `coord::chunk` — sector positions in the grid. Used in `Sector`. |
| `coord/declarations.h` | Scalar typedefs (`tile_t = int64_t`, `chunk_t = int32_t`) |
| `coord/coord.h.template` | Code-generated base classes |

> **WARNING**: `tile.h` and `chunk.h` include `coord_nese.gen.h` and `coord_neseup.gen.h`
> which are **code-generated at build time** from `coord.h.template`.
> These files **do not exist as static files** in the repo.
> You must either run the OpenAge build once, or hand-write replacements (see strategy below).

### 2. `util/` — Utilities

| File needed | Why |
|------------|-----|
| `util/fixed_point.h` | `util::FixedPoint<int64_t, 16>` — used for `time::time_t`. Header-only. |
| `util/hash.h+cpp` | `util::hash_combine()` — used by tile hash and field_cache. |
| `util/vector.h` | `util::Vector2s` — used in `Grid` for dimensions. Header-only. |
| `util/misc.h` | `util::mod()`, `util::div()` — used by `fixed_point.h`. Header-only. |
| `util/compiler.h` | `ENSURE()` macro and branch hints. Header-only. |
| `util/error/error.h` | Used by `misc.h`. Pulls in the logging subsystem. |

### 3. `time/time.h`
Defines `time::time_t = FixedPoint<int64_t,16>`. Only depends on `util/fixed_point.h`.

### 4. `datastructure/pairing_heap.h`
Pairing heap used as A* open list in `Pathfinder`. Header-only, self-contained.

---

## Dependency Graph

```
Pathfinder
├── Grid
│   └── Sector
│       ├── CostField        (needs time::time_t, coord::tile_delta)
│       └── Portal           (needs coord::tile_delta)
├── Integrator
│   ├── FieldCache           (needs util::hash)
│   ├── IntegrationField     (needs coord::tile_delta)
│   └── FlowField            (needs coord::tile_delta)
├── Path                     (needs coord::tile, time::time_t)
├── PortalNode
└── datastructure::PairingHeap

coord::tile / tile_delta
├── util::FixedPoint          (header-only)
├── util::hash_combine
└── coord_nese.gen.h          ← CODE-GENERATED (main obstacle)

time::time_t
└── util::FixedPoint
```

---

## Isolation Strategy

### Option A: Hand-Write Coord Replacement (Recommended)

Replace the openage coordinate system with simple structs. The pathfinder ONLY uses:
- `tile.ne` and `tile.se` (NE/SE isometric axes, both `int64_t`)
- `tile_delta` as a relative version of `tile`
- `chunk.ne` and `chunk.se` (both `int32_t`)
- Basic `+`, `-`, `==` operators

Write a single `pathfinding_coords.h` header that replaces all of `coord/`:

```cpp
// pathfinding_coords.h
namespace openage::coord {
    using tile_t  = int64_t;
    using chunk_t = int32_t;

    struct tile_delta {
        tile_t ne = 0, se = 0;
        tile_delta operator+(const tile_delta& o) const { return {ne+o.ne, se+o.se}; }
        tile_delta operator-(const tile_delta& o) const { return {ne-o.ne, se-o.se}; }
        tile_delta operator-() const { return {-ne, -se}; }
        bool operator==(const tile_delta& o) const { return ne==o.ne && se==o.se; }
    };

    struct tile {
        tile_t ne, se;
        tile operator+(const tile_delta& d) const { return {ne+d.ne, se+d.se}; }
        tile operator-(const tile_delta& d) const { return {ne-d.ne, se-d.se}; }
        tile_delta operator-(const tile& o)  const { return {ne-o.ne, se-o.se}; }
        bool operator==(const tile& o) const { return ne==o.ne && se==o.se; }
    };

    struct chunk {
        chunk_t ne, se;
        bool operator==(const chunk& o) const { return ne==o.ne && se==o.se; }
    };
}

namespace std {
    template<> struct hash<openage::coord::tile> {
        size_t operator()(const openage::coord::tile& t) const {
            return std::hash<int64_t>{}(t.ne) ^ (std::hash<int64_t>{}(t.se) << 32);
        }
    };
}
```

This eliminates `coord.h.template`, both `.gen.h` files, and `phys.h`/`pixel.h`/`scene.h`.

### Option B: Full Build Dependency

Keep everything as-is and build as part of the OpenAge CMake system — the `.gen.h` files are generated automatically. Link against the produced library from your project.

---

## Complete File Checklist (Option A)

```
# Core pathfinding (copy all .h + .cpp)
libopenage/pathfinding/types.h + types.cpp
libopenage/pathfinding/definitions.h + definitions.cpp
libopenage/pathfinding/cost_field.h + cost_field.cpp
libopenage/pathfinding/integration_field.h + integration_field.cpp
libopenage/pathfinding/flow_field.h + flow_field.cpp
libopenage/pathfinding/field_cache.h + field_cache.cpp
libopenage/pathfinding/integrator.h + integrator.cpp
libopenage/pathfinding/portal.h + portal.cpp
libopenage/pathfinding/sector.h + sector.cpp
libopenage/pathfinding/grid.h + grid.cpp
libopenage/pathfinding/path.h + path.cpp
libopenage/pathfinding/pathfinder.h + pathfinder.cpp

# Utility deps (mostly header-only)
libopenage/datastructure/pairing_heap.h
libopenage/util/fixed_point.h
libopenage/util/hash.h + hash.cpp
libopenage/util/vector.h
libopenage/util/misc.h
libopenage/util/compiler.h
libopenage/time/time.h

# Coord replacement:
[Option A] Write your own pathfinding_coords.h  (see above)
[Option B] libopenage/coord/ (full dir) + build to generate .gen.h files

# Fix error/log chain pulled in by misc.h:
Either stub ENSURE() as:  #define ENSURE(cond, msg) assert(cond)
OR copy: libopenage/error/error.h + libopenage/log/log.h + their deps
```

---

## Key Optional Simplifications

| Simplification | What to do |
|----------------|------------|
| Remove dirty-tracking | Replace `time::time_t` params in `CostField` with a simple `bool dirty` flag |
| Remove LOS pass | Set `with_los = false` everywhere; delete LOS wave code from `IntegrationField` (~200 lines) |
| Drop `legacy/` | Completely independent old A* — nothing in the flow-field system uses it |
| Drop `demo/` | Depends on the OpenAge renderer; not portable standalone |
| Stub `ENSURE()` | `#define ENSURE(cond, msg) assert(cond)` avoids pulling in `error/` + `log/` subsystems |
