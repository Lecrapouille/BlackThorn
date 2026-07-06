/**
 * @file Document.cpp
 * @brief Fast YAML document wrapper (rapidyaml).
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Yaml/Document.hpp"

#include <ryml.hpp>
#include <ryml_std.hpp>

#include <c4/charconv.hpp>
#include <c4/error.hpp>
#include <c4/yml/tree.hpp>
#include <csetjmp>
#include <fstream>
#include <stdexcept>
#include <string>

namespace bt {

namespace {

thread_local std::jmp_buf g_rymlParseJmp;
thread_local std::string g_rymlParseError;

c4::csubstr toCsubstr(std::string_view p_view)
{
    return c4::csubstr(p_view.data(), p_view.size());
}

C4_NORETURN void rymlErrorCallback(const char* p_msg,
                                   size_t p_len,
                                   ryml::Location /*p_loc*/,
                                   void* /*p_user*/)
{
    g_rymlParseError.assign(p_msg, p_len);
    std::longjmp(g_rymlParseJmp, 1);
}

ryml::Callbacks errorThrowingCallbacks()
{
    ryml::Callbacks callbacks = ryml::get_callbacks();
    callbacks.m_error = rymlErrorCallback;
    return callbacks;
}

struct ScopedRymlCallbacks
{
    ryml::Callbacks previous;

    explicit ScopedRymlCallbacks(ryml::Callbacks p_callbacks)
        : previous(ryml::get_callbacks())
    {
        ryml::set_callbacks(p_callbacks);
    }

    ~ScopedRymlCallbacks()
    {
        ryml::set_callbacks(previous);
    }
};

ryml::substr mutableBuffer(std::vector<char>& p_buffer)
{
    return ryml::substr(p_buffer.data(), p_buffer.size() - 1);
}

bool parseIntoTree(std::vector<char>& p_buffer, ryml::Tree& p_tree)
{
    g_rymlParseError.clear();
    ScopedRymlCallbacks const guard(errorThrowingCallbacks());
    p_tree = ryml::Tree(ryml::get_callbacks());

    if (setjmp(g_rymlParseJmp) == 0)
    {
        ryml::parse_in_place(mutableBuffer(p_buffer), &p_tree);
        return true;
    }

    return false;
}

bool readFile(std::string const& p_path, std::vector<char>& p_buffer)
{
    std::ifstream file(p_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }

    auto const size = file.tellg();
    if (size < 0)
    {
        return false;
    }

    p_buffer.resize(static_cast<std::size_t>(size) + 1);
    file.seekg(0);
    file.read(p_buffer.data(), size);
    p_buffer[static_cast<std::size_t>(size)] = '\0';
    return static_cast<bool>(file);
}

std::string toString(c4::csubstr p_view)
{
    return std::string(p_view.str, p_view.len);
}

} // namespace

YamlNode::YamlNode(ryml::Tree const* p_tree, std::uint32_t p_id) noexcept
    : m_tree(p_tree), m_id(p_id)
{
}

bool YamlNode::valid() const noexcept
{
    return m_tree != nullptr && m_id != kInvalidId &&
           m_tree->ref(m_id).readable();
}

ryml::ConstNodeRef YamlNode::ref() const
{
    return m_tree->ref(m_id);
}

bool YamlNode::isMap() const
{
    return valid() && ref().is_map();
}

bool YamlNode::isSeq() const
{
    return valid() && ref().is_seq();
}

bool YamlNode::isScalar() const
{
    return valid() && ref().has_val() && !ref().is_map() && !ref().is_seq();
}

bool YamlNode::empty() const
{
    return !valid() || ref().empty();
}

std::size_t YamlNode::size() const
{
    return valid() ? ref().num_children() : 0;
}

bool YamlNode::hasKey(std::string_view p_key) const
{
    return valid() && ref().has_child(toCsubstr(p_key));
}

YamlNode YamlNode::child(std::string_view p_key) const
{
    if (!hasKey(p_key))
    {
        return {};
    }
    return YamlNode{m_tree, ref()[toCsubstr(p_key)].id()};
}

YamlNode YamlNode::child(std::size_t p_index) const
{
    if (!valid() || !isSeq() || p_index >= size())
    {
        return {};
    }
    return YamlNode{m_tree, ref()[static_cast<ryml::id_type>(p_index)].id()};
}

std::string YamlNode::scalar() const
{
    if (!valid())
    {
        return {};
    }
    return toString(ref().val());
}

std::optional<bool> YamlNode::asBool() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }

    bool value = false;
    if (c4::from_chars(ref().val(), &value))
    {
        return value;
    }

    c4::csubstr const val = ref().val();
    if (val == "true" || val == "1")
    {
        return true;
    }
    if (val == "false" || val == "0")
    {
        return false;
    }
    return std::nullopt;
}

std::optional<int> YamlNode::asInt() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }

    int value = 0;
    if (c4::from_chars(ref().val(), &value))
    {
        return value;
    }
    return std::nullopt;
}

std::optional<double> YamlNode::asDouble() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }

    double value = 0.0;
    if (c4::yml::from_chars_float(ref().val(), &value))
    {
        return value;
    }
    return std::nullopt;
}

std::optional<std::size_t> YamlNode::asSize() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }

    std::size_t value = 0;
    if (c4::from_chars(ref().val(), &value))
    {
        return value;
    }
    return std::nullopt;
}

std::pair<std::string, YamlNode> YamlNode::typeEntry() const
{
    if (!isMap() || empty())
    {
        return {};
    }

    ryml::ConstNodeRef const first = ref().first_child();
    if (!first.valid())
    {
        return {};
    }

    return {toString(first.key()), YamlNode{m_tree, first.id()}};
}

void YamlNode::forEachMap(
    std::function<void(std::string_view, YamlNode)> const& p_fn) const
{
    if (!isMap())
    {
        return;
    }

    for (ryml::ConstNodeRef const child : ref().children())
    {
        p_fn(toString(child.key()), YamlNode{m_tree, child.id()});
    }
}

void YamlNode::forEachSeq(
    std::function<void(YamlNode)> const& p_fn) const
{
    if (!isSeq())
    {
        return;
    }

    for (ryml::ConstNodeRef const child : ref().children())
    {
        p_fn(YamlNode{m_tree, child.id()});
    }
}

robotik::Return<YamlDocument> YamlDocument::parseFile(std::string const& p_path)
{
    YamlDocument doc;
    if (!readFile(p_path, doc.m_buffer))
    {
        return robotik::Return<YamlDocument>::error(
            "Failed to read YAML file: " + p_path);
    }

    if (!parseIntoTree(doc.m_buffer, doc.m_tree))
    {
        return robotik::Return<YamlDocument>::error(
            "YAML parsing error: " + g_rymlParseError);
    }

    if (!doc.root().valid())
    {
        return robotik::Return<YamlDocument>::error("Empty YAML document");
    }

    return robotik::Return<YamlDocument>::success(std::move(doc));
}

robotik::Return<YamlDocument> YamlDocument::parseText(std::string p_text)
{
    YamlDocument doc;
    doc.m_buffer.assign(p_text.begin(), p_text.end());
    doc.m_buffer.push_back('\0');

    if (!parseIntoTree(doc.m_buffer, doc.m_tree))
    {
        return robotik::Return<YamlDocument>::error(
            "YAML parsing error: " + g_rymlParseError);
    }

    if (!doc.root().valid())
    {
        return robotik::Return<YamlDocument>::error("Empty YAML document");
    }

    return robotik::Return<YamlDocument>::success(std::move(doc));
}

YamlNode YamlDocument::root() const
{
    if (m_tree.empty())
    {
        return {};
    }
    return YamlNode{&m_tree, m_tree.rootref().id()};
}

} // namespace bt
