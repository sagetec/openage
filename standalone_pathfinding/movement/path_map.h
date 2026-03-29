// movement/path_map.h
// Standalone replacement for openage's gamestate/map.h
//
// In OpenAge, Map owns the Pathfinder and is initialized from a Terrain object
// + nyan database.  Here we strip all of that and expose a simple API:
//
//   PathMap map(grid_count, grid_size_tiles, sector_stride);
//   map.set_cost(grid_id, sector_index, tile_index, cost);
//   map.build_portals();            // call once after all costs are set
//   auto &pf = map.get_pathfinder();
//
// ─────────────────────────────────────────────────────────────────────────────

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../pathfinding/pathfinder.h"
#include "../pathfinding/grid.h"
#include "../pathfinding/sector.h"
#include "../pathfinding/cost_field.h"
#include "../pathfinding/types.h"
#include "../util/vector.h"
#include "../time/time.h"

namespace standalone {

/**
 * PathMap owns a Pathfinder and one or more movement grids.
 *
 * Analogous to openage::gamestate::Map but with zero engine dependencies.
 *
 * Typical usage:
 *
 *   // Create a 10×10 sector grid, each sector is 16×16 tiles.
 *   PathMap map;
 *   auto grid_id = map.add_grid({10, 10}, 16);   // returns 0
 *
 *   // Set impassable border (cost = IMPASSABLE)
 *   map.set_cost(grid_id, sector_index, tile_index, openage::path::COST_IMPASSABLE);
 *
 *   // Finalize (builds portals between adjacent sectors).
 *   map.build_portals();
 *
 *   // Query
 *   auto &pathfinder = map.get_pathfinder();
 */
class PathMap {
public:
    PathMap();
    ~PathMap() = default;

    /**
     * Add a movement grid and return its grid_id.
     *
     * @param grid_size  Number of sectors along each axis (width × height).
     * @param sector_stride  Number of tiles per sector side (square sectors).
     * @return The assigned grid_id (sequential, starting at 0).
     */
    openage::path::grid_id_t add_grid(
        const openage::util::Vector2s &grid_size,
        size_t sector_stride);

    /**
     * Register a named grid (convenience wrapper for add_grid).
     * The name can be retrieved later via get_grid_id().
     */
    openage::path::grid_id_t add_named_grid(
        const std::string &name,
        const openage::util::Vector2s &grid_size,
        size_t sector_stride);

    /**
     * Look up a named grid's ID.
     * Throws std::out_of_range if the name is unknown.
     */
    openage::path::grid_id_t get_grid_id(const std::string &name) const;

    /**
     * Set the movement cost of a single tile.
     *
     * @param grid_id       Grid to modify.
     * @param sector_index  Linear index of the sector in the grid.
     * @param tile_index    Linear index of the tile within the sector.
     * @param cost          Cost value (1–254 passable; 255 = impassable).
     *
     * Call build_portals() once after all costs have been set.
     */
    void set_cost(openage::path::grid_id_t grid_id,
                  size_t sector_index,
                  size_t tile_index,
                  openage::path::cost_t cost);

    /**
     * Build (or rebuild) portals for all grids.
     * Must be called after all tile costs are set and before any path queries.
     */
    void build_portals();

    /**
     * Access the underlying Pathfinder.
     */
    const std::shared_ptr<openage::path::Pathfinder> &get_pathfinder() const;

private:
    std::shared_ptr<openage::path::Pathfinder> pathfinder;

    /// Named grid lookup  (optional — only populated via add_named_grid)
    std::unordered_map<std::string, openage::path::grid_id_t> name_to_grid;

    /// Next grid ID to assign
    openage::path::grid_id_t next_grid_id{0};
};

} // namespace standalone
