/**
 * @file Yaml.cpp
 * @brief Generic YAML parsing layer (rapidyaml wrapper).
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Builder/Yaml.hpp"

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Warith-conversion"
#endif

#include <ryml.hpp>
#include <ryml_std.hpp>

#include <c4/charconv.hpp>
#include <c4/error.hpp>
#include <c4/yml/tree.hpp>

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
#include <csetjmp>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace bt {

// Yaml.hpp cannot name ryml::id_type without including rapidyaml, so it stores
// node indices as std::size_t. Catch a rapidyaml built with a narrower
// RYML_ID_TYPE here rather than through silent truncations.
static_assert(std::is_same_v<ryml::id_type, std::size_t>,
              "rapidyaml must keep its default RYML_ID_TYPE (size_t)");

// ****************************************************************************
//! \brief Parsed YAML owned by a \ref YamlDocument.
//!
//! Heap allocated, so that moving the document leaves the tree in place and the
//! \ref YamlNode views pointing at it valid.
// ****************************************************************************
struct YamlDocument::Impl
{
    //! \brief Backing storage parsed in place by rapidyaml (NUL-terminated).
    std::vector<char> buffer;
    //! \brief Parsed node tree; views into \ref buffer.
    ryml::Tree tree;
};

// ---------------------------------------------------------------------------
// rapidyaml error handling
//
// By default, rapidyaml calls abort() on parse errors. BlackThorn needs a
// recoverable Return<T>::error() instead. rapidyaml exposes a global error
// callback (must never return). We combine:
//   1. setjmp()  — save a checkpoint before parsing
//   2. longjmp() — jump back to that checkpoint from the error callback
//
// std::jmp_buf holds the CPU/register state restored by longjmp().
// ---------------------------------------------------------------------------

// ****************************************************************************
//! \brief Checkpoint and message used to recover from a rapidyaml parse error.
// ****************************************************************************
struct ParseRecovery
{
    //! \brief Where longjmp() resumes, saved by setjmp() before parsing.
    std::jmp_buf checkpoint;
    //! \brief Message reported by the rapidyaml error callback.
    std::string message;
};

// ---------------------------------------------------------------------------
//! \brief Recovery slot of the calling thread.
//! \return A slot of its own per thread, so that concurrent parses on
//!         different threads do not clash.
// ---------------------------------------------------------------------------
static ParseRecovery& parseRecovery()
{
    static thread_local ParseRecovery recovery;
    return recovery;
}

static c4::csubstr toCsubstr(std::string_view p_view)
{
    return c4::csubstr(p_view.data(), p_view.size());
}

// The signature, void* user pointer included, is imposed by
// ryml::Callbacks::m_error_parse.
C4_NORETURN static void
rymlErrorCallback(c4::csubstr p_msg,
                  ryml::ErrorDataParse const& /*p_errdata*/,
                  void* /*p_user*/) // NOSONAR: signature imposed by rapidyaml
{
    ParseRecovery& recovery = parseRecovery();
    recovery.message.assign(p_msg.str, p_msg.len);
    std::longjmp(recovery.checkpoint, 1);
}

static ryml::Callbacks errorThrowingCallbacks()
{
    ryml::Callbacks callbacks = ryml::get_callbacks();
    callbacks.m_error_parse = rymlErrorCallback;
    return callbacks;
}

// ****************************************************************************
//! \brief Install rapidyaml callbacks for a scope and restore them on exit.
// ****************************************************************************
struct ScopedRymlCallbacks
{
    //! \brief Captured before the body runs, hence before the new callbacks are
    //! installed.
    ryml::Callbacks previous = ryml::get_callbacks();

    explicit ScopedRymlCallbacks(ryml::Callbacks const& p_callbacks)
    {
        ryml::set_callbacks(p_callbacks);
    }

    ~ScopedRymlCallbacks()
    {
        ryml::set_callbacks(previous);
    }

    // Restoring the callbacks twice would be wrong, so the guard cannot be
    // duplicated.
    ScopedRymlCallbacks(ScopedRymlCallbacks const&) = delete;
    ScopedRymlCallbacks& operator=(ScopedRymlCallbacks const&) = delete;
    ScopedRymlCallbacks(ScopedRymlCallbacks&&) = delete;
    ScopedRymlCallbacks& operator=(ScopedRymlCallbacks&&) = delete;
};

static ryml::substr mutableBuffer(std::vector<char>& p_buffer)
{
    return ryml::substr(p_buffer.data(), p_buffer.size() - 1);
}

static bool parseIntoTree(std::vector<char>& p_buffer, ryml::Tree& p_tree)
{
    ParseRecovery& recovery = parseRecovery();
    recovery.message.clear();
    ScopedRymlCallbacks const guard(errorThrowingCallbacks());
    p_tree = ryml::Tree(ryml::get_callbacks());

    if (setjmp(recovery.checkpoint) == 0)
    {
        ryml::parse_in_place(mutableBuffer(p_buffer), &p_tree);
        return true;
    }

    return false;
}

static bool readFile(std::string const& p_path, std::vector<char>& p_buffer)
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

static std::string toString(c4::csubstr p_view)
{
    return std::string(p_view.str, p_view.len);
}

template <typename T>
static std::optional<T> parseScalarAs(c4::csubstr p_val)
{
    // The whole scalar must be consumed. c4::from_chars() reports success as
    // soon as a numeric prefix is found, which would silently turn the string
    // "1;2;3" into the number 1.
    if (T value{}; c4::from_chars_first(p_val, &value) == p_val.len)
    {
        return value;
    }
    return std::nullopt;
}

static std::optional<double> parseDoubleScalar(c4::csubstr p_val)
{
    if (auto value = parseScalarAs<double>(p_val))
    {
        return value;
    }

    // YAML also spells the special reals ".nan", ".inf" and "-.inf". Only these
    // non-numeric forms are left to rapidyaml, whose float parser would
    // otherwise accept a numeric prefix and read "1;2;3" as 1.
    if (p_val.begins_with('.') || p_val.begins_with("-."))
    {
        if (double value = 0.0; c4::yml::from_chars_float(p_val, &value))
        {
            return value;
        }
    }
    return std::nullopt;
}

static std::optional<bool> parseBoolScalar(c4::csubstr p_val)
{
    if (bool value = false; c4::from_chars(p_val, &value))
    {
        return value;
    }

    if (p_val == "true" || p_val == "1")
    {
        return true;
    }
    if (p_val == "false" || p_val == "0")
    {
        return false;
    }
    return std::nullopt;
}

YamlNode::YamlNode(ryml::Tree const* p_tree, std::size_t p_id) noexcept
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

bool YamlNode::isQuotedScalar() const
{
    return isScalar() && ref().is_val_quoted();
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
    return parseBoolScalar(ref().val());
}

std::optional<int> YamlNode::asInt() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }
    return parseScalarAs<int>(ref().val());
}

std::optional<double> YamlNode::asDouble() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }
    return parseDoubleScalar(ref().val());
}

std::optional<std::size_t> YamlNode::asSize() const
{
    if (!isScalar())
    {
        return std::nullopt;
    }
    return parseScalarAs<std::size_t>(ref().val());
}

std::pair<std::string, YamlNode> YamlNode::typeEntry() const
{
    if (!isMap() || empty())
    {
        return {};
    }

    ryml::ConstNodeRef const first = ref().first_child();
    if (!first.readable())
    {
        return {};
    }

    return {toString(first.key()), YamlNode{m_tree, first.id()}};
}

// Type erasure through std::function, rather than a template parameter, is what
// keeps this loop out of Yaml.hpp: it needs the complete rapidyaml types, which
// the header deliberately only forward declares.
void YamlNode::forEachMap(
    std::function<void(std::string_view, YamlNode)> const& p_fn) const // NOSONAR
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
    std::function<void(YamlNode)> const& p_fn) const // NOSONAR
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

YamlDocument::YamlDocument() : m_impl(std::make_unique<Impl>())
{
}

YamlDocument::~YamlDocument() = default;

YamlDocument::YamlDocument(YamlDocument&&) noexcept = default;

YamlDocument& YamlDocument::operator=(YamlDocument&&) noexcept = default;

Return<YamlDocument> YamlDocument::parseFile(std::string const& p_path)
{
    YamlDocument doc;
    if (!readFile(p_path, doc.m_impl->buffer))
    {
        return Return<YamlDocument>::error(
            "Failed to read YAML file: " + p_path);
    }

    if (!parseIntoTree(doc.m_impl->buffer, doc.m_impl->tree))
    {
        return Return<YamlDocument>::error(
            "YAML parsing error: " + parseRecovery().message);
    }

    if (!doc.root().valid())
    {
        return Return<YamlDocument>::error("Empty YAML document");
    }

    return Return<YamlDocument>::success(std::move(doc));
}

Return<YamlDocument> YamlDocument::parseText(std::string p_text)
{
    YamlDocument doc;
    doc.m_impl->buffer.assign(p_text.begin(), p_text.end());
    doc.m_impl->buffer.push_back('\0');

    if (!parseIntoTree(doc.m_impl->buffer, doc.m_impl->tree))
    {
        return Return<YamlDocument>::error(
            "YAML parsing error: " + parseRecovery().message);
    }

    if (!doc.root().valid())
    {
        return Return<YamlDocument>::error("Empty YAML document");
    }

    return Return<YamlDocument>::success(std::move(doc));
}

YamlNode YamlDocument::root() const
{
    if ((m_impl == nullptr) || m_impl->tree.empty())
    {
        return {};
    }
    return YamlNode{&m_impl->tree, m_impl->tree.rootref().id()};
}

bool YamlDocument::hasKey(std::string_view p_key) const
{
    return root().hasKey(p_key);
}

YamlNode YamlDocument::operator[](std::string_view p_key) const
{
    return root().child(p_key);
}

bool scalarReadsAsNonString(std::string_view p_text)
{
    if (p_text.empty())
    {
        return false;
    }

    c4::csubstr const val(p_text.data(), p_text.size());
    return parseScalarAs<int>(val).has_value() ||
           parseDoubleScalar(val).has_value() ||
           parseBoolScalar(val).has_value();
}

} // namespace bt
