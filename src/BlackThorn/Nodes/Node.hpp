/**
 * @file Node.hpp
 * @brief Base class for all behavior tree nodes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"
#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Blackboard/Ports.hpp"
#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Visitors/Visitor.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace bt {

// ****************************************************************************
//! \brief Enum representing the status of a node in the behavior tree.
// ****************************************************************************
enum class Status
{
    INVALID = 0, //!< The node is invalid (internal use only).
    RUNNING = 1, //!< The node is running.
    SUCCESS = 2, //!< The node is successful.
    FAILURE = 3, //!< The node is failed.
};

// ---------------------------------------------------------------------------
//! \brief Convert a node status to a string.
//! \param[in] p_status The status to convert.
//! \return The string representation of the status.
// ---------------------------------------------------------------------------
inline std::string const& to_string(Status p_status)
{
    static std::array<std::string, 5> const names = {
        "INVALID", "RUNNING", "SUCCESS", "FAILURE", "???"};
    const auto idx = std::min<std::size_t>(static_cast<std::size_t>(p_status),
                                           names.size() - 1);
    return names[idx];
}

// ****************************************************************************
//! \brief Static and callback configuration for one tree node slot.
//!
//! Stored in parallel to \ref Tree::config() and read by the tree interpreter
//! during tick, reset, and validation.
// ****************************************************************************
struct NodeConfig
{
    //! \brief Minimum successful children required by \c Parallel.
    int parallel_min_success = 1;
    //! \brief Minimum failed children required by \c Parallel.
    int parallel_min_fail = 1;
    //! \brief Whether \c ParallelAll requires all children to succeed.
    bool parallel_success_on_all = true;
    //! \brief Whether \c ParallelAll requires all children to fail.
    bool parallel_fail_on_all = true;

    //! \brief Default repetition count for \c Repeater.
    std::size_t repeater_default = 0;
    //! \brief Maximum attempts for \c UntilSuccess / \c UntilFailure (0 =
    //! infinite).
    std::size_t until_attempts = 0;
    //! \brief Default duration in milliseconds for temporal nodes.
    std::size_t duration_ms = 1000;

    //! \brief Blackboard key written by \c SetBlackboard.
    std::string set_blackboard_key;
    //! \brief Blackboard value written by \c SetBlackboard.
    std::string set_blackboard_value;

    //! \brief Tick callback for \ref CallbackLeaf nodes.
    std::function<Status()> callback;
    //! \brief Predicate callback for \c Condition nodes.
    std::function<bool()> condition;
    //! \brief Optional reset callback for callback/condition nodes.
    std::function<void()> on_reset;
};

//! \brief Invalid index sentinel for nodes not yet bound to a tree slot.
inline constexpr std::size_t INVALID_NODE_INDEX = SIZE_MAX;

class ConstBehaviorTreeVisitor;
class BehaviorTreeVisitor;
class Tree;

// ****************************************************************************
//! \brief Base class for all nodes in the behavior tree.
//!
//! Nodes are stored in a flat pool inside \ref Tree. Each node holds the
//! index of its slot and delegates tick/reset/halt execution to the owning
//! tree interpreter.
// ****************************************************************************
class Node
{
    friend class Tree;

public:

    // ------------------------------------------------------------------------
    //! \brief Built-in node kinds interpreted by \ref Tree.
    // ------------------------------------------------------------------------
    enum class Kind : uint8_t
    {
        Sequence = 0,
        ReactiveSequence,
        SequenceWithMemory,
        Selector,
        ReactiveSelector,
        SelectorWithMemory,
        Parallel,
        ParallelAll,
        Inverter,
        ForceSuccess,
        ForceFailure,
        RunOnce,
        Repeater,
        UntilSuccess,
        UntilFailure,
        Timeout,
        Delay,
        Cooldown,
        Success,
        Failure,
        Wait,
        SetBlackboard,
        Condition,
        Callback,
        SubTree,
    };

    using Ptr = std::unique_ptr<Node>;

    // ------------------------------------------------------------------------
    //! \brief Flat storage for behavior tree node instances.
    //!
    //! Nodes created through \ref create() are constructed with placement new
    //! inside an internal buffer and destroyed when the pool is cleared.
    //! Nodes created externally are adopted through \ref adopt() and kept in a
    //! separate heap ownership vector.
    // ------------------------------------------------------------------------
    class Pool
    {
    public:

        Pool() = default;

        ~Pool()
        {
            clear();
        }

        Pool(Pool const&) = delete;
        Pool& operator=(Pool const&) = delete;
        Pool(Pool&&) noexcept = default;
        Pool& operator=(Pool&&) noexcept = default;

        // --------------------------------------------------------------------
        //! \brief Construct a node in-place inside the pool.
        //! \tparam T Concrete node type derived from \ref Node.
        //! \param[in] p_args Arguments forwarded to the node constructor.
        //! \return Reference to the newly created node.
        // --------------------------------------------------------------------
        template <typename T, typename... Args>
        T& create(Args&&... p_args)
        {
            static_assert(std::is_base_of_v<Node, T>);
            static_assert(sizeof(T) <= c_max_size);
            static_assert(alignof(T) <= c_max_align);

            m_placed.emplace_back();
            PlacedBlock& block = m_placed.back();
            T* node = new (block.storage) T(std::forward<Args>(p_args)...);
            block.destroy = [](Node* p_node) { static_cast<T*>(p_node)->~T(); };
            m_nodes.push_back(node);
            return *node;
        }

        // --------------------------------------------------------------------
        //! \brief Take ownership of an externally allocated node.
        //! \param[in] p_node Node to insert into the pool.
        //! \return Reference to the adopted node.
        // --------------------------------------------------------------------
        Node& adopt(Ptr p_node)
        {
            Node* raw = p_node.get();
            m_heap.push_back(std::move(p_node));
            m_nodes.push_back(raw);
            return *raw;
        }

        // --------------------------------------------------------------------
        //! \brief Access a node by its pool index.
        //! \param[in] p_index Pool index.
        //! \return Reference to the node.
        // --------------------------------------------------------------------
        [[nodiscard]] Node& operator[](std::size_t p_index)
        {
            return *m_nodes[p_index];
        }

        // --------------------------------------------------------------------
        //! \brief Access a node by its pool index (const).
        //! \param[in] p_index Pool index.
        //! \return Const reference to the node.
        // --------------------------------------------------------------------
        [[nodiscard]] Node const& operator[](std::size_t p_index) const
        {
            return *m_nodes[p_index];
        }

        // --------------------------------------------------------------------
        //! \brief Return the flat pointer vector exposed to visitors.
        //! \return Vector of node pointers in insertion order.
        // --------------------------------------------------------------------
        [[nodiscard]] std::vector<Node*> const& nodes() const noexcept
        {
            return m_nodes;
        }

        // --------------------------------------------------------------------
        //! \brief Return the number of nodes stored in the pool.
        //! \return Node count.
        // --------------------------------------------------------------------
        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_nodes.size();
        }

        // --------------------------------------------------------------------
        //! \brief Destroy all nodes and clear the pool.
        // --------------------------------------------------------------------
        void clear()
        {
            for (PlacedBlock& block : m_placed)
            {
                if (block.destroy != nullptr)
                {
                    block.destroy(reinterpret_cast<Node*>(block.storage));
                }
            }
            m_placed.clear();
            m_heap.clear();
            m_nodes.clear();
        }

        // --------------------------------------------------------------------
        //! \brief Pre-allocate storage for upcoming node creations.
        // --------------------------------------------------------------------
        void reserve(std::size_t p_count)
        {
            m_nodes.reserve(p_count);
        }

    private:

        //! \brief Maximum size of a node object constructed in-place.
        static constexpr std::size_t c_max_size = 512;
        //! \brief Maximum alignment required by in-place node storage.
        static constexpr std::size_t c_max_align = alignof(std::max_align_t);

        //! \brief Buffer block used for placement-new node construction.
        struct PlacedBlock
        {
            alignas(c_max_align) std::byte storage[c_max_size];
            void (*destroy)(Node*) = nullptr;
        };

        //! \brief In-place constructed nodes destroyed on pool clear.
        std::deque<PlacedBlock> m_placed;
        //! \brief Heap-owned nodes adopted from outside the pool.
        std::vector<Ptr> m_heap;
        //! \brief Flat pointer list indexed by tree slots.
        std::vector<Node*> m_nodes;
    };

    // ------------------------------------------------------------------------
    //! \brief Destructor needed because of virtual methods.
    // ------------------------------------------------------------------------
    virtual ~Node() = default;

    // ------------------------------------------------------------------------
    //! \brief Return the storage index assigned by the owning tree.
    //! \return Node index, or \ref INVALID_NODE_INDEX if not bound yet.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::size_t index() const
    {
        return m_index;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the tree that owns this node.
    //! \return Pointer to the owning tree, or nullptr if not bound yet.
    // ------------------------------------------------------------------------
    [[nodiscard]] Tree* ownerTree() const
    {
        return m_tree;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the visualizer ID for this node slot.
    //! \return Visualizer ID stored in the owning tree metadata.
    // ------------------------------------------------------------------------
    [[nodiscard]] uint32_t visualizerId() const;

    // ------------------------------------------------------------------------
    //! \brief Bind this node to a tree slot.
    //! \param[in] p_tree Owning tree.
    //! \param[in] p_index Storage index inside the tree pool.
    // ------------------------------------------------------------------------
    void bindToTree(Tree& p_tree, std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Get the ports provided by the node.
    //! \return The ports provided by the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual PortList providedPorts() const
    {
        return PortList{};
    }

    // ------------------------------------------------------------------------
    //! \brief Execute the current node through the owning tree.
    //! \details Delegates to \ref Tree::tickNode(), which performs setup,
    //! run, and teardown according to the node kind.
    //! \return The status of the node (SUCCESS, FAILURE, RUNNING).
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tick();

    // ------------------------------------------------------------------------
    //! \brief Get the runtime status of the node.
    //! \return The status stored in the owning tree runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status status() const;

    // ------------------------------------------------------------------------
    //! \brief Get the type name of the node.
    //! \return String derived from the node kind registered in the tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] char const* typeName() const;

    // ------------------------------------------------------------------------
    //! \brief Reset the node and its descendants through the owning tree.
    // ------------------------------------------------------------------------
    virtual void reset();

    // ------------------------------------------------------------------------
    //! \brief Halt the node and its descendants through the owning tree.
    // ------------------------------------------------------------------------
    virtual void halt();

    // ------------------------------------------------------------------------
    //! \brief Check whether the node is valid inside the owning tree.
    //! \return True if the node is valid, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual bool isValid() const;

    // ------------------------------------------------------------------------
    //! \brief Accept a const visitor (read-only).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    virtual void accept(ConstBehaviorTreeVisitor& p_visitor) const = 0;

    // ------------------------------------------------------------------------
    //! \brief Accept a non-const visitor (read-write).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    virtual void accept(BehaviorTreeVisitor& p_visitor) = 0;

    // ------------------------------------------------------------------------
    //! \brief Get the blackboard for the node.
    //! \return The blackboard for the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Blackboard::Ptr blackboard() const
    {
        return m_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Assign a blackboard to the node.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    void setBlackboard(Blackboard::Ptr const& p_blackboard)
    {
        m_blackboard = p_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Configure the port remapping for this node.
    //! Maps port names to blackboard keys (e.g., "target" -> "${move_goal}").
    //! \param[in] p_remapping The port remapping configuration.
    // ------------------------------------------------------------------------
    void setPortRemapping(
        std::unordered_map<std::string, std::string> const& p_remapping)
    {
        m_resolved_ports = resolvePortRemapping(p_remapping);
    }

    void setResolvedPorts(ResolvedPortMap const& p_ports)
    {
        m_resolved_ports = p_ports;
    }

    // ------------------------------------------------------------------------
    //! \brief Public accessor to a remapped input port.
    //! \param[in] p_port Port name.
    //! \return Resolved value, or std::nullopt if not found.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] std::optional<T>
    getInputPublic(std::string const& p_port) const
    {
        return getInput<T>(p_port);
    }

    // ------------------------------------------------------------------------
    //! \brief Fill the parallel node configuration slot in the owning tree.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    virtual void fillConfig(NodeConfig& /*p_config*/) const {}

    // ------------------------------------------------------------------------
    //! \brief Return the node kind used when adopting a factory-built node.
    //! \return Node kind registered for this concrete type.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual Kind registrationKind() const
    {
        return Kind::Sequence;
    }

    //! \brief The name of the node.
    std::string name;

protected: // Port management

    // ------------------------------------------------------------------------
    //! \brief Get an input from the port.
    //! Resolves the port name to a blackboard key using port remapping.
    //! \param[in] p_port The port to get the input from.
    //! \return The input value, or std::nullopt if not found.
    // ------------------------------------------------------------------------
    template <typename T>
    std::optional<T> getInput(std::string const& p_port) const
    {
        if (!m_blackboard)
        {
            return std::nullopt;
        }

        if (auto it = m_resolved_ports.find(p_port); it != m_resolved_ports.end())
        {
            return VariableResolver::resolveBinding<T>(it->second, *m_blackboard);
        }

        return m_blackboard->get<T>(p_port);
    }

    template <typename T>
    void setOutput(std::string const& p_port, T&& p_value)
    {
        if (!m_blackboard)
        {
            return;
        }

        PortBinding const* binding = nullptr;
        if (auto it = m_resolved_ports.find(p_port); it != m_resolved_ports.end())
        {
            binding = &it->second;
        }

        if (binding && binding->kind == PortBindingKind::BlackboardKey)
        {
            m_blackboard->set(binding->data, std::forward<T>(p_value));
            return;
        }

        std::string const key =
            binding ? binding->data : p_port;
        m_blackboard->set(key, std::forward<T>(p_value));
    }

protected: // Lifecycle hooks

    // ------------------------------------------------------------------------
    //! \brief Hook invoked when the node leaves the RUNNING state.
    //! \details By default nothing is done, override to handle cleanup logic.
    //! \param[in] p_status The status of the node (SUCCESS or FAILURE).
    // ------------------------------------------------------------------------
    virtual void onTearDown(Status p_status)
    {
        (void)p_status;
    }

    // ------------------------------------------------------------------------
    //! \brief Hook invoked when halt() is called on a RUNNING node.
    //! \details By default nothing is done, override to handle interruption.
    // ------------------------------------------------------------------------
    virtual void onHalt() {}

    // ------------------------------------------------------------------------
    //! \brief Return the owning tree (non-const).
    //! \pre The node is bound to a tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] Tree& tree();

    // ------------------------------------------------------------------------
    //! \brief Return the owning tree (const).
    //! \pre The node is bound to a tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] Tree const& tree() const;

    // ------------------------------------------------------------------------
    //! \brief Forward tear-down hook to derived classes from the tree.
    //! \param[in] p_status Final status reported by the interpreter.
    // ------------------------------------------------------------------------
    void onTearDownPublic(Status p_status)
    {
        onTearDown(p_status);
    }

    // ------------------------------------------------------------------------
    //! \brief Forward halt hook to derived classes from the tree.
    // ------------------------------------------------------------------------
    void onHaltPublic()
    {
        onHalt();
    }

    // ------------------------------------------------------------------------
    //! \brief Custom validation hook for node kinds with extra state.
    //! \return True if node-specific state is valid.
    // ------------------------------------------------------------------------
    [[nodiscard]] virtual bool isValidCustom() const
    {
        return true;
    }

    //! \brief Maps concrete node types to \ref Kind (specialized per node).
    template <typename T, typename = void>
    struct KindTraits;

    //! \brief Storage index inside the owning tree pool.
    std::size_t m_index = INVALID_NODE_INDEX;
    //! \brief Owning tree, set by \ref bindToTree().
    Tree* m_tree = nullptr;
    //! \brief The blackboard for the node (shared data store).
    Blackboard::Ptr m_blackboard = nullptr;
    //! \brief The port remapping for this node (port name -> blackboard key).
    ResolvedPortMap m_resolved_ports;
};

using NodeKind = Node::Kind;

// ---------------------------------------------------------------------------
//! \brief Convert a node kind to its YAML/type name.
//! \param[in] p_kind Node kind to convert.
//! \return Null-terminated type name string.
// ---------------------------------------------------------------------------
[[nodiscard]] char const* toString(NodeKind p_kind);

// ---------------------------------------------------------------------------
//! \brief Check whether a node kind is a composite node.
//! \param[in] p_kind Node kind to test.
//! \return True for sequence/selector/parallel kinds.
// ---------------------------------------------------------------------------
[[nodiscard]] bool isComposite(NodeKind p_kind);

// ---------------------------------------------------------------------------
//! \brief Check whether a node kind is a decorator node.
//! \param[in] p_kind Node kind to test.
//! \return True for inverter/repeater/temporal kinds.
// ---------------------------------------------------------------------------
[[nodiscard]] bool isDecorator(NodeKind p_kind);

template <typename T, typename = void>
struct NodeKindTraits;

// ---------------------------------------------------------------------------
//! \brief Resolve the \ref NodeKind associated with a concrete node type.
//! \tparam T Concrete node type.
//! \return Compile-time node kind used by \ref Tree::emplaceNode().
// ---------------------------------------------------------------------------
template <typename T>
inline constexpr NodeKind nodeKindOf()
{
    return NodeKindTraits<T>::value;
}

} // namespace bt
