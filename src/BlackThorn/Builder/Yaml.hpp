/**
 * @file Yaml.hpp
 * @brief Generic YAML parsing layer (rapidyaml wrapper).
 *
 * Pipeline: \c YamlDocument / YamlNode → \ref TreeDocument / Builder.
 * Parse error handling: see \c Yaml.cpp.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Common/Return.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ryml.hpp>

namespace bt {

class YamlDocument;

// ****************************************************************************
//! \brief Non-owning view into a node of a \ref YamlDocument.
//!
//! \warning Do not store a YamlNode after its parent \ref YamlDocument is
//!          destroyed.
// ****************************************************************************
class YamlNode
{
    friend class YamlDocument;

public:

    // ------------------------------------------------------------------------
    //! \brief Build an invalid node (points to no document).
    // ------------------------------------------------------------------------
    YamlNode() = default;

    // ------------------------------------------------------------------------
    //! \brief Whether the node refers to a readable slot in a live document.
    //! \return \c true if the node can be safely queried.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool valid() const noexcept;

    // ------------------------------------------------------------------------
    //! \brief Whether the node is a mapping (\c key: value pairs).
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isMap() const;

    // ------------------------------------------------------------------------
    //! \brief Whether the node is a sequence (ordered list of items).
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isSeq() const;

    // ------------------------------------------------------------------------
    //! \brief Whether the node is a leaf scalar (neither map nor sequence).
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isScalar() const;

    // ------------------------------------------------------------------------
    //! \brief Whether the node holds no children.
    //! \return \c true for an invalid node or a container with no elements.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool empty() const;

    // ------------------------------------------------------------------------
    //! \brief Number of direct children.
    //! \return Child count for maps/sequences, \c 0 for scalars/invalid nodes.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::size_t size() const;

    // ------------------------------------------------------------------------
    //! \brief Whether a mapping contains the given key.
    //! \param[in] p_key Key to look up.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool hasKey(std::string_view p_key) const;

    // ------------------------------------------------------------------------
    //! \brief Access a child of a mapping by key.
    //! \param[in] p_key Key to look up.
    //! \return The child node, or an invalid node when the key is absent.
    // ------------------------------------------------------------------------
    [[nodiscard]] YamlNode child(std::string_view p_key) const;

    // ------------------------------------------------------------------------
    //! \brief Access a child of a sequence by position.
    //! \param[in] p_index Zero-based index into the sequence.
    //! \return The child node, or an invalid node when out of range.
    // ------------------------------------------------------------------------
    [[nodiscard]] YamlNode child(std::size_t p_index) const;

    // ------------------------------------------------------------------------
    //! \brief Raw textual content of a scalar node.
    //! \return The scalar text, or an empty string for invalid nodes.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string scalar() const;

    // ------------------------------------------------------------------------
    //! \brief Interpret the scalar as a boolean.
    //! \return The parsed value, or \c std::nullopt when not a valid boolean.
    //! \note Accepts native YAML booleans plus \c "1" / \c "0".
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<bool> asBool() const;

    // ------------------------------------------------------------------------
    //! \brief Interpret the scalar as an \c int.
    //! \return The parsed value, or \c std::nullopt on failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<int> asInt() const;

    // ------------------------------------------------------------------------
    //! \brief Interpret the scalar as a \c double.
    //! \return The parsed value, or \c std::nullopt on failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<double> asDouble() const;

    // ------------------------------------------------------------------------
    //! \brief Interpret the scalar as a \c std::size_t.
    //! \return The parsed value, or \c std::nullopt on failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<std::size_t> asSize() const;

    // ------------------------------------------------------------------------
    //! \brief Extract \c {TypeName, content} from a single-key map node.
    //!
    //! Behavior-tree definitions encode the node type as the (single) map key,
    //! e.g. \c "Sequence: { ... }" yields \c {"Sequence", <the inner map>}.
    //! \return The key/child pair, or an empty pair when the node is not a
    //!         non-empty mapping.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::pair<std::string, YamlNode> typeEntry() const;

    // ------------------------------------------------------------------------
    //! \brief Invoke \p p_fn for each \c key/value entry of a mapping.
    //! \param[in] p_fn Callback receiving the key view and the value node.
    //! \note Does nothing when the node is not a mapping. The key view is only
    //!       valid for the duration of the callback.
    // ------------------------------------------------------------------------
    void forEachMap(
        std::function<void(std::string_view, YamlNode)> const& p_fn) const;

    // ------------------------------------------------------------------------
    //! \brief Invoke \p p_fn for each item of a sequence.
    //! \param[in] p_fn Callback receiving each element node.
    //! \note Does nothing when the node is not a sequence.
    // ------------------------------------------------------------------------
    void forEachSeq(std::function<void(YamlNode)> const& p_fn) const;

private:

    // ------------------------------------------------------------------------
    //! \brief Bind a view to node \p p_id inside tree \p p_tree.
    // ------------------------------------------------------------------------
    YamlNode(ryml::Tree const* p_tree, ryml::id_type p_id) noexcept;

    // ------------------------------------------------------------------------
    //! \brief Underlying rapidyaml handle for the referenced node.
    // ------------------------------------------------------------------------
    [[nodiscard]] ryml::ConstNodeRef ref() const;

    //! \brief Owning tree, or \c nullptr for an invalid node.
    ryml::Tree const* m_tree = nullptr;
    //! \brief Index of the referenced node within \ref m_tree.
    ryml::id_type m_id = ryml::NONE;
    //! \brief Sentinel marking a node index as unset.
    static constexpr ryml::id_type kInvalidId = ryml::NONE;
};

// ****************************************************************************
//! \brief Owns parsed YAML text and exposes the document root.
//!
//! Use \ref TreeDocument when loading a behavior tree (sections
//! \c BehaviorTree, \c Blackboard, \c SubTrees).
// ****************************************************************************
class YamlDocument
{
public:

    // ------------------------------------------------------------------------
    //! \brief Parse a YAML file from disk.
    //! \param[in] p_path Path to the YAML file to read.
    //! \return The parsed document on success, or an error describing the
    //!         read/parse failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<YamlDocument>
    parseFile(std::string const& p_path);

    // ------------------------------------------------------------------------
    //! \brief Parse YAML from an in-memory string.
    //! \param[in] p_text YAML text (taken by value; copied into the document).
    //! \return The parsed document on success, or an error on parse failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] static robotik::Return<YamlDocument>
    parseText(std::string p_text);

    // ------------------------------------------------------------------------
    //! \brief View of the document's root node.
    //! \return An invalid node when the document is empty.
    // ------------------------------------------------------------------------
    [[nodiscard]] YamlNode root() const;

    // ------------------------------------------------------------------------
    //! \brief Whether the root mapping contains the given key.
    //! \param[in] p_key Key to look up.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool hasKey(std::string_view p_key) const;

    // ------------------------------------------------------------------------
    //! \brief Access a child of the root mapping by key.
    //! \param[in] p_key Key to look up.
    //! \return The child node, or an invalid node when the key is absent.
    // ------------------------------------------------------------------------
    [[nodiscard]] YamlNode operator[](std::string_view p_key) const;

private:

    //! \brief Backing storage parsed in place by rapidyaml (NUL-terminated).
    std::vector<char> m_buffer;
    //! \brief Parsed node tree; views into \ref m_buffer.
    ryml::Tree m_tree;
};

} // namespace bt
