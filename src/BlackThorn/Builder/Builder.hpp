/**
 * @file Builder.hpp
 * @brief Behavior tree construction from YAML.
 *
 * Loading pipeline:
 * \code
 *   YamlDocument::parseFile()   ← raw YAML bytes (Yaml.hpp)
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
#include "BlackThorn/Builder/Yaml.hpp"

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

    // ------------------------------------------------------------------------
    //! \brief Parse a behavior-tree YAML file and index BT-specific sections.
    //! \param[in] p_path Path to the YAML file.
    //! \return Parsed document on success, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<std::shared_ptr<TreeDocument>>
    parseFile(std::string const& p_path);

    // ------------------------------------------------------------------------
    //! \brief Parse behavior-tree YAML from an in-memory string.
    //! \param[in] p_text YAML text.
    //! \return Parsed document on success, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<std::shared_ptr<TreeDocument>>
    parseText(std::string p_text);

    // ------------------------------------------------------------------------
    //! \brief Build a runnable \ref Tree from this parsed document.
    //! \param[in] p_factory Node factory used to instantiate C++ node types.
    //! \param[in] p_blackboard Optional root blackboard (created when null).
    //! \param[in] p_options Instantiation flags (merge, reserve, lazy subtrees).
    //! \return Instantiated tree on success, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] robotik::Return<Tree::Ptr>
    instantiate(NodeFactory const& p_factory,
                Blackboard::Ptr p_blackboard = nullptr,
                BuilderOptions p_options = {}) const;

    // ------------------------------------------------------------------------
    //! \brief Access the underlying generic YAML document.
    //! \return Parsed YAML kept alive for node views and lazy subtree builds.
    // ------------------------------------------------------------------------
    [[nodiscard]] YamlDocument const& yaml() const noexcept
    {
        return m_yaml;
    }

    // ------------------------------------------------------------------------
    //! \brief Subtree definitions indexed from the \c SubTrees YAML section.
    //! \return Registry of named subtree roots.
    // ------------------------------------------------------------------------
    [[nodiscard]] SubTreeRegistry const& subtrees() const noexcept
    {
        return m_subtrees;
    }

    // ------------------------------------------------------------------------
    //! \brief Whether the YAML defines an explicit \c Blackboard section.
    //! \return \c true when a blackboard block was found at parse time.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool hasBlackboardSection() const noexcept
    {
        return m_hasBlackboard;
    }

private:

    //! \brief Parsed YAML bytes and node tree.
    YamlDocument m_yaml;
    //! \brief Named subtree definitions extracted from \c SubTrees.
    SubTreeRegistry m_subtrees;
    //! \brief Set when a top-level \c Blackboard section is present.
    bool m_hasBlackboard = false;
};

// ****************************************************************************
//! \brief Creates \ref Tree instances from YAML definitions.
// ****************************************************************************
class Builder
{
public:

    // ------------------------------------------------------------------------
    //! \brief Parse a YAML file and instantiate a behavior tree in one step.
    //! \param[in] p_factory Node factory used to create C++ node types.
    //! \param[in] p_file_path Path to the YAML definition file.
    //! \param[in] p_blackboard Optional root blackboard (created when null).
    //! \param[in] p_options Instantiation flags.
    //! \return Instantiated tree on success, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<Tree::Ptr>
    fromFile(NodeFactory const& p_factory,
             std::string const& p_file_path,
             Blackboard::Ptr p_blackboard = nullptr,
             BuilderOptions p_options = {});

    // ------------------------------------------------------------------------
    //! \brief Parse YAML text and instantiate a behavior tree in one step.
    //! \param[in] p_factory Node factory used to create C++ node types.
    //! \param[in] p_yaml_text YAML definition text.
    //! \param[in] p_blackboard Optional root blackboard (created when null).
    //! \param[in] p_options Instantiation flags.
    //! \return Instantiated tree on success, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<Tree::Ptr>
    fromText(NodeFactory const& p_factory,
             std::string const& p_yaml_text,
             Blackboard::Ptr p_blackboard = nullptr,
             BuilderOptions p_options = {});

    // ------------------------------------------------------------------------
    //! \brief Recursively instantiate a single YAML node into \p p_tree.
    //! \param[in,out] p_tree Tree receiving the new node(s).
    //! \param[in] p_factory Node factory used to create C++ node types.
    //! \param[in] p_node YAML node to parse (\c TypeName: { ... } form).
    //! \param[in] p_blackboard Blackboard scope for the created node(s).
    //! \param[in] p_subtrees Optional registry for \c SubTree references.
    //! \param[in] p_options Instantiation flags.
    //! \return Number of nodes created, or an error message.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<uint32_t>
    parseYAMLNode(Tree& p_tree,
                  NodeFactory const& p_factory,
                  YamlNode const& p_node,
                  Blackboard::Ptr p_blackboard = nullptr,
                  SubTreeRegistry const* p_subtrees = nullptr,
                  BuilderOptions p_options = {});
};

} // namespace bt
