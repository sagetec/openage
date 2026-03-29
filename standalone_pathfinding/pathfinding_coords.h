// pathfinding_coords.h
// Drop-in replacement for openage's coord/ module.
// The pathfinding system only uses ne/se axis values and basic arithmetic.
// Replace coord/tile.h, coord/chunk.h and their code-generated dependencies
// with this single header.

#pragma once

#include <cstdint>
#include <functional> // for std::hash
#include <ostream>    // for operator<<

namespace openage {
namespace coord {

// ── Scalar types ──────────────────────────────────────────────────────────────
using tile_t  = int64_t;
using chunk_t = int32_t;

// ── Lightweight physical (world-space) 2D coordinate ────────────────────────
// Used only for heuristic distance calculations in pathfinder.cpp.
// In OpenAge, phys2 is a FixedPoint-based type. For our standalone use,
// double precision is sufficient.
struct phys2 {
    double ne = 0.0;
    double se = 0.0;

    constexpr phys2() = default;
    constexpr phys2(double ne, double se) : ne{ne}, se{se} {}

    constexpr phys2 operator+(const phys2 &o) const { return {ne + o.ne, se + o.se}; }
    constexpr phys2 operator-(const phys2 &o) const { return {ne - o.ne, se - o.se}; }

    double length() const {
        double sq = ne * ne + se * se;
        // cheap integer sqrt — pathfinder only needs relative ordering
        return sq > 0.0 ? static_cast<double>(static_cast<int>(sq * 1000000.0)) / 1000000.0
                        : 0.0;  // avoid <cmath> dependency; caller casts to int anyway
    }
};

// ── Relative tile offset (direction / delta) ──────────────────────────────────
struct tile_delta {
    tile_t ne = 0;  ///< North-East axis offset
    tile_t se = 0;  ///< South-East axis offset

    constexpr tile_delta() = default;
    constexpr tile_delta(tile_t ne, tile_t se) : ne{ne}, se{se} {}

    constexpr tile_delta operator+(const tile_delta &o) const { return {ne + o.ne, se + o.se}; }
    constexpr tile_delta operator-(const tile_delta &o) const { return {ne - o.ne, se - o.se}; }
    constexpr tile_delta operator-()                    const { return {-ne, -se}; }
    constexpr tile_delta operator*(tile_t s)            const { return {ne * s, se * s}; }
    constexpr tile_delta operator/(tile_t s)            const { return {ne / s, se / s}; }

    constexpr tile_delta &operator+=(const tile_delta &o) { ne += o.ne; se += o.se; return *this; }
    constexpr tile_delta &operator-=(const tile_delta &o) { ne -= o.ne; se -= o.se; return *this; }

    constexpr bool operator==(const tile_delta &o) const { return ne == o.ne && se == o.se; }
    constexpr bool operator!=(const tile_delta &o) const { return !(*this == o); }

    /// Convert to world-space physics coord for heuristic distance.
    phys2 to_phys2() const { return {static_cast<double>(ne), static_cast<double>(se)}; }
};

// ── Absolute tile position ─────────────────────────────────────────────────────
struct tile {
    tile_t ne;  ///< North-East axis
    tile_t se;  ///< South-East axis

    // NOTE: No default constructor — matches openage's design intent
    //       (zero-init of absolute coord is origin-sensitive).
    constexpr tile(tile_t ne, tile_t se) : ne{ne}, se{se} {}

    constexpr tile       operator+(const tile_delta &d) const { return {ne + d.ne, se + d.se}; }
    constexpr tile       operator-(const tile_delta &d) const { return {ne - d.ne, se - d.se}; }
    constexpr tile_delta operator-(const tile &o)       const { return {ne - o.ne, se - o.se}; }

    constexpr tile &operator+=(const tile_delta &d) { ne += d.ne; se += d.se; return *this; }
    constexpr tile &operator-=(const tile_delta &d) { ne -= d.ne; se -= d.se; return *this; }

    constexpr bool operator==(const tile &o) const { return ne == o.ne && se == o.se; }
    constexpr bool operator!=(const tile &o) const { return !(*this == o); }

    /// Convert to world-space physics coord for heuristic distance calculations.
    phys2 to_phys2() const { return {static_cast<double>(ne), static_cast<double>(se)}; }
};

// ── Chunk (sector position in the grid) ───────────────────────────────────────
struct chunk_delta {
    chunk_t ne = 0;
    chunk_t se = 0;

    constexpr chunk_delta() = default;
    constexpr chunk_delta(chunk_t ne, chunk_t se) : ne{ne}, se{se} {}
    constexpr bool operator==(const chunk_delta &o) const { return ne == o.ne && se == o.se; }
};

struct chunk {
    chunk_t ne;
    chunk_t se;

    constexpr chunk(chunk_t ne, chunk_t se) : ne{ne}, se{se} {}
    constexpr bool operator==(const chunk &o) const { return ne == o.ne && se == o.se; }
    constexpr bool operator!=(const chunk &o) const { return !(*this == o); }

    /// Returns the absolute tile position of the top-left corner of this chunk.
    /// sector_size is the number of tiles per side in a sector/grid cell.
    tile to_tile(size_t sector_size) const {
        return {static_cast<tile_t>(ne) * static_cast<tile_t>(sector_size),
                static_cast<tile_t>(se) * static_cast<tile_t>(sector_size)};
    }
};

} // namespace coord
} // namespace openage

// ── Stream operators (for log messages) ───────────────────────────────────────
// These must be outside the coord namespace so ADL finds them when
// streaming into std::ostream or MessageBuilder.

inline std::ostream &operator<<(std::ostream &os, const openage::coord::tile &t) {
    return os << "(" << t.ne << ", " << t.se << ")";
}

inline std::ostream &operator<<(std::ostream &os, const openage::coord::tile_delta &d) {
    return os << "[" << d.ne << ", " << d.se << "]";
}

inline std::ostream &operator<<(std::ostream &os, const openage::coord::chunk &c) {
    return os << "{chunk " << c.ne << ", " << c.se << "}";
}

// ── std::hash specialisations ─────────────────────────────────────────────────
namespace std {

template <>
struct hash<openage::coord::tile> {
    size_t operator()(const openage::coord::tile &t) const noexcept {
        // Simple combine: XOR with rotation
        size_t h1 = std::hash<openage::coord::tile_t>{}(t.ne);
        size_t h2 = std::hash<openage::coord::tile_t>{}(t.se);
        return h1 ^ (h2 * 2654435761ULL);
    }
};

template <>
struct hash<openage::coord::tile_delta> {
    size_t operator()(const openage::coord::tile_delta &d) const noexcept {
        size_t h1 = std::hash<openage::coord::tile_t>{}(d.ne);
        size_t h2 = std::hash<openage::coord::tile_t>{}(d.se);
        return h1 ^ (h2 * 2654435761ULL);
    }
};

} // namespace std
