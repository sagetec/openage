// movement/path_map.cpp

#include "path_map.h"

#include <stdexcept>

namespace standalone {

PathMap::PathMap()
    : pathfinder{std::make_shared<openage::path::Pathfinder>()} {}

openage::path::grid_id_t PathMap::add_grid(
    const openage::util::Vector2s &grid_size,
    size_t sector_stride)
{
    auto id = next_grid_id++;
    auto grid = std::make_shared<openage::path::Grid>(id, grid_size, sector_stride);
    pathfinder->add_grid(grid);
    return id;
}

openage::path::grid_id_t PathMap::add_named_grid(
    const std::string &name,
    const openage::util::Vector2s &grid_size,
    size_t sector_stride)
{
    auto id = add_grid(grid_size, sector_stride);
    name_to_grid[name] = id;
    return id;
}

openage::path::grid_id_t PathMap::get_grid_id(const std::string &name) const {
    return name_to_grid.at(name);
}

void PathMap::set_cost(openage::path::grid_id_t grid_id,
                       size_t sector_index,
                       size_t tile_index,
                       openage::path::cost_t cost)
{
    auto grid   = pathfinder->get_grid(grid_id);
    auto sector = grid->get_sector(sector_index);
    auto cf     = sector->get_cost_field();
    cf->set_cost(tile_index, cost, openage::time::TIME_ZERO);
}

void PathMap::build_portals() {
    // Iterate over all grids registered in the pathfinder
    size_t num_grids = static_cast<size_t>(next_grid_id);
    for (openage::path::grid_id_t id = 0; id < num_grids; ++id) {
        auto grid = pathfinder->get_grid(id);
        grid->init_portals();
        grid->init_portal_nodes();
    }
}

const std::shared_ptr<openage::path::Pathfinder> &PathMap::get_pathfinder() const {
    return pathfinder;
}

} // namespace standalone
