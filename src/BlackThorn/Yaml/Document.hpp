/**
 * @file Document.hpp
 * @brief Generic YAML parsing layer (rapidyaml wrapper).
 *
 * BlackThorn splits YAML handling into two layers:
 *
 * \code
 *   YamlDocument / YamlNode     ← generic YAML (this file)
 *        ↓
 *   TreeDocument / Builder    ← behavior-tree semantics (Builder.hpp)
 * \endcode
 *
 * \ref YamlDocument owns the file buffer and parsed tree.
 * \ref YamlNode is a non-owning view; it becomes invalid when the document
 * is destroyed.
 *
 * Example — read a config file without building a behavior tree:
 * \code
 *   auto doc = bt::YamlDocument::parseFile("settings.yaml");
 *   if (!doc) { return; }
 *
 *   if (doc->hasKey("timeout")) {
 *       auto ms = doc->root().child("timeout").asInt();
 *   }
 *
 *   doc->root().forEachMap([](std::string_view key, bt::YamlNode value) {
 *       std::cout << key << " = " << value.scalar() << '\n';
 *   });
 * \endcode
 *
 * Example — inspect a behavior-tree node definition:
 * \code
 *   // YAML:  Sequence:
 *   //           name: Root
 *   //           children: [...]
 *   auto [type, content] = node.typeEntry();
 *   // type == "Sequence", content.hasKey("children") == true
 * \endcode
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
public:

    YamlNode() = default;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool isMap() const;
    [[nodiscard]] bool isSeq() const;
    [[nodiscard]] bool isScalar() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] bool hasKey(std::string_view p_key) const;
    [[nodiscard]] YamlNode child(std::string_view p_key) const;
    [[nodiscard]] YamlNode child(std::size_t p_index) const;

    [[nodiscard]] std::string scalar() const;
    [[nodiscard]] std::optional<bool> asBool() const;
    [[nodiscard]] std::optional<int> asInt() const;
    [[nodiscard]] std::optional<double> asDouble() const;
    [[nodiscard]] std::optional<std::size_t> asSize() const;

    //! \brief Extract \c {TypeName, content} from a single-key map node.
    [[nodiscard]] std::pair<std::string, YamlNode> typeEntry() const;

    void forEachMap(
        std::function<void(std::string_view, YamlNode)> const& p_fn) const;
    void forEachSeq(std::function<void(YamlNode)> const& p_fn) const;

private:

    friend class YamlDocument;

    YamlNode(ryml::Tree const* p_tree, std::uint32_t p_id) noexcept;

    [[nodiscard]] ryml::ConstNodeRef ref() const;

    ryml::Tree const* m_tree = nullptr;
    std::uint32_t m_id = 0;
    static constexpr std::uint32_t kInvalidId = static_cast<std::uint32_t>(-1);
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

    [[nodiscard]] static robotik::Return<YamlDocument>
    parseFile(std::string const& p_path);

    [[nodiscard]] static robotik::Return<YamlDocument>
    parseText(std::string p_text);

    [[nodiscard]] YamlNode root() const;

    [[nodiscard]] bool hasKey(std::string_view p_key) const
    {
        return root().hasKey(p_key);
    }

    [[nodiscard]] YamlNode operator[](std::string_view p_key) const
    {
        return root().child(p_key);
    }

private:

    std::vector<char> m_buffer;
    ryml::Tree m_tree;
};

} // namespace bt
