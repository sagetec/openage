// stubs/coord/phys.h
// Stub: the legacy/ A* files reference coord/phys.h for phys2/phys3 types.
// Since pathfinding_coords.h now defines coord::phys2, we just forward-include it.
// phys3 (3D world coordinate) is not needed by the flow-field pathfinder.
#pragma once

#include "../pathfinding_coords.h"  // coord::phys2 is already defined there

namespace openage {
namespace coord {

// phys3 — used only in legacy A* code, not in flow-field pathfinding.
struct phys3 {
    double ne = 0.0;
    double se = 0.0;
    double up = 0.0;

    phys3 operator+(const phys3 &o) const { return {ne+o.ne, se+o.se, up+o.up}; }
    phys3 operator-(const phys3 &o) const { return {ne-o.ne, se-o.se, up-o.up}; }

    double length() const {
        double sq = ne*ne + se*se + up*up;
        return sq > 0.0 ? static_cast<double>(static_cast<int>(sq * 1000000.0)) / 1000000.0 : 0.0;
    }
};

} // namespace coord
} // namespace openage
