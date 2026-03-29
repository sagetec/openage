cmake_minimum_required(VERSION 3.16)
project(standalone_pathfinding CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# /FS: serialize PDB writes so parallel cl.exe instances don't collide (MSVC C1041)
if(MSVC)
    add_compile_options(/FS)
endif()

# ── Portability stubs ─────────────────────────────────────────────────────────
# OpenAge's misc.h includes error/error.h for the ENSURE() macro.
# We stub that out with a simple assert so we don't need the full error/log chain.
add_definitions(-DSTANDALONE_PATHFINDING_STUB_ERRORS)

# ── Include paths ─────────────────────────────────────────────────────────────
# Pathfinding sources use includes like:
#   #include "coord/tile.h"
#   #include "pathfinding/types.h"
#   #include "util/fixed_point.h"
# So we put the root of this directory on the path, plus the stubs dir which
# provides shims for coord/, error/, log/.
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}        # pathfinding_coords.h, util/, time/, etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs  # coord/tile.h → pathfinding_coords.h, error/, log/
)

# ── Core pathfinding library ──────────────────────────────────────────────────
add_library(pathfinding STATIC
    # Core flow-field pathfinder
    pathfinding/types.cpp
    pathfinding/definitions.cpp
    pathfinding/cost_field.cpp
    pathfinding/integration_field.cpp
    pathfinding/flow_field.cpp
    pathfinding/field_cache.cpp
    pathfinding/integrator.cpp
    pathfinding/portal.cpp
    pathfinding/sector.cpp
    pathfinding/grid.cpp
    pathfinding/path.cpp
    pathfinding/pathfinder.cpp

    # Utilities required by pathfinding
    util/hash.cpp
    util/compiler.cpp
)

target_include_directories(pathfinding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

# ── Legacy A* (disabled) ──────────────────────────────────────────────────────
# The legacy/ A* implementation depends on util/strings.h which is a
# Python-generated header in OpenAge and is not included in this standalone
# package.  The legacy system is completely independent of the flow-field
# pathfinder — nothing in the main pathfinding library uses it.
#
# To build it you would need to either:
#   a) Copy util/strings.h from a built OpenAge tree, or
#   b) Write a stub (it only provides openage::util::to_string<T>()).
#
# add_library(pathfinding_legacy STATIC
#     pathfinding/legacy/a_star.cpp
#     pathfinding/legacy/heuristics.cpp
#     pathfinding/legacy/path.cpp
# )
# target_include_directories(pathfinding_legacy PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/stubs
# )

# ── Movement / Avoidance layer ────────────────────────────────────────────────
# Standalone equivalents of OpenAge's:
#   gamestate/map.h+cpp          → movement/path_map.h+cpp
#   gamestate/system/move.h+cpp  → movement/move_system.h (header-only)
add_library(movement STATIC
    movement/path_map.cpp
    movement/move_system.cpp   # intentionally empty; move_system.h is header-only
)

target_include_directories(movement PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

target_link_libraries(movement PUBLIC pathfinding)

# ── Demo executable ───────────────────────────────────────────────────────────
add_executable(pathfinding_demo main_demo.cpp)
target_link_libraries(pathfinding_demo PRIVATE pathfinding movement)
target_include_directories(pathfinding_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)
