/**
 * @file TreeMetadata.hpp
 * @brief Visualizer metadata indexed by node slot in a Tree.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bt {

// ****************************************************************************
//! \brief Visualizer IDs indexed by node slot in a Tree.
// ****************************************************************************
struct TreeMetadata
{
    std::vector<uint32_t> visualizer_ids;
    uint32_t next_visualizer_id = 1;

    void reserve(std::size_t p_count)
    {
        visualizer_ids.reserve(p_count);
    }

    void pushSlot()
    {
        visualizer_ids.push_back(0);
    }

    void setVisualizerId(std::size_t p_index, uint32_t p_id)
    {
        assert(p_index < visualizer_ids.size());
        visualizer_ids[p_index] = p_id;
        if (p_id >= next_visualizer_id)
        {
            next_visualizer_id = p_id + 1;
        }
    }

    void assignVisualizerId(std::size_t p_index)
    {
        setVisualizerId(p_index, next_visualizer_id++);
    }

    [[nodiscard]] uint32_t visualizerId(std::size_t p_index) const
    {
        assert(p_index < visualizer_ids.size());
        return visualizer_ids[p_index];
    }
};

} // namespace bt
