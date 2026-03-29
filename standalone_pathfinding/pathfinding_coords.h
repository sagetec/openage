// pathfinding_coords.h
// Drop-in replacement for openage's coord/ module.
// The pathfinding system only uses ne/se axis values and basic arithmetic.
// Replace coord/tile.h, coord/chunk.h and their code-generated dependencies
// with this single header.

#pragma once

#include <cstdint>
#include <functional> // for std::hash

namespace openage {
namespace coord {

// ── Scalar types ──────────────────────────────────────────────────────────────
using tile_t  = int64_t;
using chunk_t = int32_t;

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
};

} // namespace coord
} // namespace openage

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
