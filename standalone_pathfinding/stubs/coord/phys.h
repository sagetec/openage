// stubs/coord/phys.h
// Stub: provides minimal phys2/phys3 needed by the legacy/ A* files.
// The flow-field pathfinder does NOT use phys types.
#pragma once

#include <cmath>
#include <cstdint>
#include "coord/tile.h"   // pulls in pathfinding_coords.h via stub

namespace openage {
namespace coord {

// Floating-point 2-D physical coordinate (NE/SE axes)
struct phys2 {
    double ne = 0.0;
    double se = 0.0;

    phys2 operator+(const phys2 &o) const { return {ne + o.ne, se + o.se}; }
    phys2 operator-(const phys2 &o) const { return {ne - o.ne, se - o.se}; }
    bool operator==(const phys2 &o) const { return ne == o.ne && se == o.se; }
};

// Floating-point 3-D physical coordinate (NE/SE/UP axes)
struct phys3 {
    double ne = 0.0;
    double se = 0.0;
    double up = 0.0;

    phys3 operator+(const phys3 &o) const { return {ne+o.ne, se+o.se, up+o.up}; }
    phys3 operator-(const phys3 &o) const { return {ne-o.ne, se-o.se, up-o.up}; }
    bool operator==(const phys3 &o) const {
        return ne == o.ne && se == o.se && up == o.up;
    }

    // Convert to tile (floor each axis)
    openage::coord::tile to_tile() const {
        return { static_cast<openage::coord::tile_t>(ne),
                 static_cast<openage::coord::tile_t>(se) };
    }

    // Center of the tile this position lies on
    phys3 to_phys3_center() const {
        return { static_cast<double>(static_cast<openage::coord::tile_t>(ne)) + 0.5,
                 static_cast<double>(static_cast<openage::coord::tile_t>(se)) + 0.5,
                 0.0 };
    }

    double length() const {
        return std::sqrt(ne*ne + se*se + up*up);
    }

    // Direction vector (normalized)
    phys3 to_angle() const { return *this; } // placeholder for legacy compatibility
};

} // namespace coord
} // namespace openage
