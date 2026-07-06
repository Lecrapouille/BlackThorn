/**
 * @file Builder.hpp
 * @brief Behavior tree construction from YAML.
 *
 * Loading pipeline:
 * \code
 *   YamlDocument::parseFile()   ← raw YAML bytes (Yaml/Document.hpp)
 *        ↓
 *   TreeDocument::parseFile()   ← BT metadata (SubTrees, Blackboard flag)
 *        ↓
 *   TreeDocument::instantiate() ← C++ nodes (Sequence, Action, …)
 *        ↓
 *   Tree::tick()
 * \endcode
 *
 * Convenience shortcut — parse + instantiate in one call:
 * \code
 *   auto result = bt::Builder::fromFile(factory, "Patrol.yaml", blackboard);
 *   if (result.isSuccess()) {
 *       result.getValue()->tick();
 *   }
 * \endcode
 *
 * Split parse / build (for benchmarks or caching):
 * \code
 *   auto doc = bt::TreeDocument::parseFile("Patrol.yaml");
 *   auto tree = doc.getValue()->instantiate(factory, blackboard);
 * \endcode
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
//! \brief Named subtree definitions from the YAML \c SubTrees section.
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
    bool mergeBlackboard = true;
    bool assignVisualizerIds = true;
    bool reserveNodes = true;
    bool lazySubTrees = false;
};

// ****************************************************************************
//! \brief Parsed behavior-tree YAML kept alive for lazy subtree builds.
//!
//! Wraps a \ref YamlDocument and pre-indexes BT-specific sections.
//! Call \ref instantiate() to create a runnable \ref Tree.
//!
//! \code
//!   auto doc = bt::TreeDocument::parseFile("patrol.yaml");
//!   if (!doc) { return; }
//!
//!   auto tree = doc.getValue()->instantiate(factory, blackboard);
//!   if (tree) { tree.getValue()->tick(); }
//! \endcode
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

// ****************************************************************************
//! \brief Creates \ref Tree instances from YAML definitions.
// ****************************************************************************
class Builder
{
public:

    [[nodiscard]] static robotik::Return<Tree::Ptr>
    fromFile(NodeFactory const& p_factory,
             std::string const& p_file_path,
             Blackboard::Ptr p_blackboard = nullptr,
             BuilderOptions p_options = {});

    [[nodiscard]] static robotik::Return<Tree::Ptr>
    fromText(NodeFactory const& p_factory,
             std::string const& p_yaml_text,
             Blackboard::Ptr p_blackboard = nullptr,
             BuilderOptions p_options = {});

    [[nodiscard]] static robotik::Return<uint32_t>
    parseYAMLNode(Tree& p_tree,
                  NodeFactory const& p_factory,
                  YamlNode const& p_node,
                  Blackboard::Ptr p_blackboard = nullptr,
                  SubTreeRegistry const* p_subtrees = nullptr,
                  BuilderOptions p_options = {});
};

} // namespace bt
