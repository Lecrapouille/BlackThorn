/**
 * @file TreeDocument.hpp
 * @brief Parsed behavior tree YAML with separate instantiate step.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Builder/Factory.hpp"
#include "BlackThorn/Common/Return.hpp"
#include "BlackThorn/Nodes/Tree.hpp"
#include "BlackThorn/Yaml/Document.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief Registry of reusable subtree definitions from YAML.
// ****************************************************************************
struct SubTreeRegistry
{
    std::unordered_map<std::string, YamlNode> definitions;
};

// ****************************************************************************
//! \brief Options controlling tree instantiation from a parsed document.
// ****************************************************************************
struct BuilderOptions
{
    //! \brief Merge YAML Blackboard section into the provided blackboard.
    bool mergeBlackboard = true;
    //! \brief Assign visualizer IDs during build (disable for load benchmarks).
    bool assignVisualizerIds = true;
    //! \brief Pre-size node pool from a YAML node count pass.
    bool reserveNodes = true;
    //! \brief Defer nested subtree construction until first tick.
    bool lazySubTrees = false;
};

// ****************************************************************************
//! \brief Parsed tree YAML kept alive for lazy subtree instantiation.
// ****************************************************************************
class TreeDocument
{
public:

    [[nodiscard]] static robotik::Return<std::shared_ptr<TreeDocument>>
    parseFile(std::string const& p_path);

    [[nodiscard]] static robotik::Return<std::shared_ptr<TreeDocument>>
    parseText(std::string p_text);

    [[nodiscard]] robotik::Return<Tree::Ptr>
    instantiate(NodeFactory const& p_factory,
                Blackboard::Ptr p_blackboard = nullptr,
                BuilderOptions p_options = {}) const;

    [[nodiscard]] YamlDocument const& yaml() const noexcept
    {
        return m_yaml;
    }

    [[nodiscard]] SubTreeRegistry const& subtrees() const noexcept
    {
        return m_subtrees;
    }

    [[nodiscard]] bool hasBlackboardSection() const noexcept
    {
        return m_hasBlackboard;
    }

private:

    YamlDocument m_yaml;
    SubTreeRegistry m_subtrees;
    bool m_hasBlackboard = false;
};

} // namespace bt
