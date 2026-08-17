/**
 * @file Yaml.hpp
 * @brief Generic YAML parsing layer (rapidyaml wrapper).
 *
 * Pipeline: \c YamlDocument / YamlNode → \ref TreeDocument / Builder.
 * Parse error handling: see \c Yaml.cpp.
 *
 * rapidyaml is an implementation detail: this header only forward declares the
 * two class names it needs, so a project consuming BlackThorn neither includes
 * \c <ryml.hpp> nor needs its include path.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Common/Return.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// Mirrors <c4/yml/fwd.hpp>, so that a translation unit including <ryml.hpp>
// afterwards sees consistent declarations. Note that ryml is a namespace made
// of `using namespace c4::yml`, hence the declarations below use c4::yml.
namespace c4::yml {
class Tree;
class ConstNodeRef;
} // namespace c4::yml

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
    //! \brief Whether the scalar was written between quotes in the source.
    //!
    //! Quoting carries a meaning in YAML: \c "42" is a string while \c 42 is an
    //! integer. Readers inferring a type from the text must not infer one here.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isQuotedScalar() const;

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

    //! \brief Sentinel marking a node index as unset (rapidyaml's \c NONE).
    static constexpr std::size_t kInvalidId = static_cast<std::size_t>(-1);

    // ------------------------------------------------------------------------
    //! \brief Bind a view to node \p p_id inside tree \p p_tree.
    // ------------------------------------------------------------------------
    YamlNode(c4::yml::Tree const* p_tree, std::size_t p_id) noexcept;

    // ------------------------------------------------------------------------
    //! \brief Underlying rapidyaml handle for the referenced node.
    //! \note Declared with an incomplete return type on purpose: only Yaml.cpp,
    //!       which includes rapidyaml, can call it.
    // ------------------------------------------------------------------------
    [[nodiscard]] c4::yml::ConstNodeRef ref() const;

    //! \brief Owning tree, or \c nullptr for an invalid node.
    c4::yml::Tree const* m_tree = nullptr;
    //! \brief Index of the referenced node within \ref m_tree.
    std::size_t m_id = kInvalidId;
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
    //! \brief Build an empty document, whose \ref root() is invalid.
    // ------------------------------------------------------------------------
    YamlDocument();

    // ------------------------------------------------------------------------
    //! \brief Release the parsed tree and its backing storage.
    // ------------------------------------------------------------------------
    ~YamlDocument();

    // ------------------------------------------------------------------------
    //! \brief Transfer ownership of the parsed tree.
    //! \note \ref YamlNode views taken from \p p_other stay valid, as the tree
    //!       itself does not move.
    // ------------------------------------------------------------------------
    YamlDocument(YamlDocument&& p_other) noexcept;

    // ------------------------------------------------------------------------
    //! \brief Transfer ownership of the parsed tree.
    // ------------------------------------------------------------------------
    YamlDocument& operator=(YamlDocument&& p_other) noexcept;

    //! \brief Not copyable: the tree holds views into the backing storage, so a
    //! copy would read the bytes owned by the original document.
    YamlDocument(YamlDocument const&) = delete;
    YamlDocument& operator=(YamlDocument const&) = delete;

    // ------------------------------------------------------------------------
    //! \brief Parse a YAML file from disk.
    //! \param[in] p_path Path to the YAML file to read.
    //! \return The parsed document on success, or an error describing the
    //!         read/parse failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] static Return<YamlDocument>
    parseFile(std::string const& p_path);

    // ------------------------------------------------------------------------
    //! \brief Parse YAML from an in-memory string.
    //! \param[in] p_text YAML text (taken by value; copied into the document).
    //! \return The parsed document on success, or an error on parse failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] static Return<YamlDocument>
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

    //! \brief Backing storage and parsed node tree, defined in Yaml.cpp so that
    //! rapidyaml stays out of this header.
    struct Impl;
    //! \brief Never null on a document that was not moved from.
    std::unique_ptr<Impl> m_impl;
};

// ----------------------------------------------------------------------------
//! \brief Whether a plain scalar would be read back as a number or a boolean.
//! \param[in] p_text Scalar text, without quotes.
//!
//! Writers use this to decide whether a string needs quotes to survive a round
//! trip: the text \c 42 would come back as an integer, \c "42" as a string. It
//! answers with the very parsers used by \ref YamlNode, so both sides cannot
//! drift apart.
// ----------------------------------------------------------------------------
[[nodiscard]] bool scalarReadsAsNonString(std::string_view p_text);

} // namespace bt
