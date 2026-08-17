/**
 * @file Tree.hpp
 * @brief Tree container and flat node storage.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Network/TreeMetadata.hpp"
#include "BlackThorn/Nodes/Node.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace bt {

class VisualizerClient;
class SubTreeNode;

// ****************************************************************************
//! \brief Owns and runs a behavior tree instance.
//!
//! A Tree stores all of its nodes in a flat pool (\ref nodes()). Parent
//! and child relationships are expressed through indices stored inside
//! composite and decorator nodes, not through pointers.
//!
//! Typical workflow:
//! - create the tree with \ref create() or \ref Tree::Tree "Tree()";
//! - attach a blackboard if needed;
//! - build the node graph with \ref createRoot(), \ref emplaceNode(), or
//!   composite helpers such as Sequence::addChild();
//! - call \ref tick() on each update.
//!
//! \code{.cpp}
//! using namespace bt;
//!
//! auto tree = Tree::create();
//! auto& root = tree->createRoot<Sequence>();
//! root.addChild<Success>();
//! root.addChild<Failure>();
//!
//! Status status = tree->tick();
//! \endcode
//!
//! When a node is created through the Factory, transfer ownership with
//! \ref adoptNode():
//!
//! \code{.cpp}
//! auto tree = Tree::create();
//! auto node = factory.create("MyAction", blackboard);
//! size_t index = tree->adoptNode(std::move(node));
//! tree->setRootIndex(index);
//! \endcode
// ****************************************************************************
class Tree
{
public:

    using Ptr = std::unique_ptr<Tree>;

    // ------------------------------------------------------------------------
    //! \brief Structure-of-Arrays runtime state for behavior trees.
    // ------------------------------------------------------------------------
    struct Runtime
    {
        std::vector<Status> statuses;
        //! \brief Child indices grouped by parent node slot.
        std::vector<std::vector<std::size_t>> children;
        std::vector<std::size_t> child_cursors;
        std::vector<std::size_t> decorator_children;

        std::vector<std::size_t> counters;
        std::vector<std::size_t> limits;
        std::vector<Status> cached_statuses;
        std::vector<bool> flags;
        std::vector<std::chrono::steady_clock::time_point> time_points;
        std::vector<std::size_t> duration_ms_runtime;

        // --------------------------------------------------------------------
        //! \brief Allocate a new runtime slot for a node.
        // --------------------------------------------------------------------
        void pushSlot()
        {
            statuses.push_back(Status::INVALID);
            children.emplace_back();
            child_cursors.push_back(0);
            decorator_children.push_back(INVALID_NODE_INDEX);
            counters.push_back(0);
            limits.push_back(0);
            cached_statuses.push_back(Status::INVALID);
            flags.push_back(false);
            time_points.push_back(std::chrono::steady_clock::time_point{});
            duration_ms_runtime.push_back(0);
        }

        // --------------------------------------------------------------------
        //! \brief Register a child index for a composite parent slot.
        // --------------------------------------------------------------------
        void appendChild(std::size_t p_parent, size_t p_child)
        {
            children[p_parent].push_back(p_child);
        }

        // --------------------------------------------------------------------
        //! \brief Register the single child of a decorator slot.
        // --------------------------------------------------------------------
        void setDecoratorChild(std::size_t p_parent, size_t p_child)
        {
            decorator_children[p_parent] = p_child;
        }

        // --------------------------------------------------------------------
        //! \brief Return the number of children for a composite slot.
        // --------------------------------------------------------------------
        [[nodiscard]] size_t childCount(std::size_t p_index) const
        {
            return children[p_index].size();
        }

        // --------------------------------------------------------------------
        //! \brief Return a child index for a composite slot.
        // --------------------------------------------------------------------
        [[nodiscard]] size_t childAt(std::size_t p_index, size_t p_offset) const
        {
            return children[p_index][p_offset];
        }

        [[nodiscard]] bool flag(std::size_t p_index) const
        {
            return flags[p_index];
        }

        void setFlag(std::size_t p_index, bool p_value)
        {
            flags[p_index] = p_value;
        }

        // --------------------------------------------------------------------
        //! \brief Pre-allocate runtime slots for upcoming nodes.
        // --------------------------------------------------------------------
        void reserve(std::size_t p_count)
        {
            statuses.reserve(p_count);
            children.reserve(p_count);
            child_cursors.reserve(p_count);
            decorator_children.reserve(p_count);
            counters.reserve(p_count);
            limits.reserve(p_count);
            cached_statuses.reserve(p_count);
            flags.reserve(p_count);
            time_points.reserve(p_count);
            duration_ms_runtime.reserve(p_count);
        }
    };

    // ------------------------------------------------------------------------
    //! \brief Create an empty tree.
    // ------------------------------------------------------------------------
    static Ptr create()
    {
        return std::make_unique<Tree>();
    }

    Tree() = default;
    Tree(Tree&&) noexcept = default;
    Tree& operator=(Tree&&) noexcept = default;
    Tree(Tree const&) = delete;
    Tree& operator=(Tree const&) = delete;

    // ------------------------------------------------------------------------
    //! \brief Construct a node in-place and register it in the storage.
    //! \tparam T Concrete node type derived from \ref Node.
    //! \param p_args Arguments forwarded to the node constructor.
    //! \return Reference to the newly created node.
    //!
    //! The node is appended at the end of the storage and receives a stable
    //! index. This method does not designate the node as the tree root; use
    //! \ref createRoot() or \ref setRootIndex() for that.
    // ------------------------------------------------------------------------
    template <typename T, typename... Args>
    [[nodiscard]] T& emplaceNode(Args&&... p_args)
    {
        static_assert(std::is_base_of_v<Node, T>, "T must inherit from Node");
        size_t index = m_pool.size();
        T& node = m_pool.create<T>(std::forward<Args>(p_args)...);
        node.bindToTree(*this, index);
        m_kinds.push_back(nodeKindOf<T>());
        m_configs.emplace_back();
        node.fillConfig(m_configs.back());
        m_runtime.pushSlot();
        m_metadata.pushSlot();
        return node;
    }

    // ------------------------------------------------------------------------
    //! \brief Take ownership of an already constructed node.
    //! \param p_node Node to insert into the storage.
    //! \return Index assigned to the node inside \ref nodes().
    //!
    //! Used when the node is built outside the tree, for example by
    //! \c NodeFactory. The node is bound to this tree and appended to the
    //! flat storage.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t adoptNode(Node::Ptr p_node)
    {
        size_t index = m_pool.size();
        Node& node = m_pool.adopt(std::move(p_node));
        node.bindToTree(*this, index);
        m_kinds.push_back(node.registrationKind());
        m_configs.emplace_back();
        node.fillConfig(m_configs.back());
        m_runtime.pushSlot();
        m_metadata.pushSlot();
        return index;
    }

    // ------------------------------------------------------------------------
    //! \brief Pre-allocate storage for an expected node count.
    //! \param[in] p_count Expected number of nodes in the tree.
    // ------------------------------------------------------------------------
    void reserveNodes(std::size_t p_count)
    {
        m_pool.reserve(p_count);
        m_kinds.reserve(p_count);
        m_configs.reserve(p_count);
        m_runtime.reserve(p_count);
        m_metadata.reserve(p_count);
    }

    // ------------------------------------------------------------------------
    //! \brief Create the root node and mark it as the entry point.
    //! \tparam T Concrete node type derived from \ref Node.
    //! \param p_args Arguments forwarded to the node constructor.
    //! \return Reference to the root node, to be ignored when the root needs no
    //! further configuration.
    // ------------------------------------------------------------------------
    template <class T, typename... Args>
    T& createRoot(Args&&... p_args)
    {
        T& root = emplaceNode<T>(std::forward<Args>(p_args)...);
        setRootIndex(root.index());
        return root;
    }

    // ------------------------------------------------------------------------
    //! \brief Set the index of the node executed by \ref tick().
    //! \param p_root_index Storage index of the root node.
    //!
    //! Required when the tree is built incrementally and the root is not
    //! necessarily the last inserted node, for example during YAML parsing
    //! or when adopting a node created by the Factory.
    // ------------------------------------------------------------------------
    void setRootIndex(std::size_t p_root_index)
    {
        m_root_index = p_root_index;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the storage index of the root node.
    //! \return Root index, or \ref INVALID_NODE_INDEX if no root was set.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t rootIndex() const
    {
        return m_root_index;
    }

    // ------------------------------------------------------------------------
    //! \brief Check whether a root node was designated.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline bool hasRoot() const
    {
        return m_root_index != INVALID_NODE_INDEX;
    }

    // ------------------------------------------------------------------------
    //! \brief Access a node by its storage index.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node& node(std::size_t p_index)
    {
        assert(p_index < m_pool.size());
        return m_pool[p_index];
    }

    [[nodiscard]] Node const& node(std::size_t p_index) const
    {
        assert(p_index < m_pool.size());
        return m_pool[p_index];
    }

    // ------------------------------------------------------------------------
    //! \brief Return the root node.
    //! \pre \ref hasRoot() is true.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node& getRoot()
    {
        assert(hasRoot());
        return node(m_root_index);
    }

    // ------------------------------------------------------------------------
    //! \brief Return the root node.
    //! \pre \ref hasRoot() is true.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node const& getRoot() const
    {
        assert(hasRoot());
        return node(m_root_index);
    }

    // ------------------------------------------------------------------------
    //! \brief Return the flat node storage vector.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::vector<Node*> const& nodes() const noexcept
    {
        return m_pool.nodes();
    }

    // ------------------------------------------------------------------------
    //! \brief Return the number of nodes stored in the pool.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t nodeCount() const noexcept
    {
        return m_pool.size();
    }

    // ------------------------------------------------------------------------
    //! \brief Return visualizer metadata indexed by node slot.
    // ------------------------------------------------------------------------
    [[nodiscard]] TreeMetadata& metadata()
    {
        return m_metadata;
    }

    // ------------------------------------------------------------------------
    //! \brief Return visualizer metadata indexed by node slot.
    // ------------------------------------------------------------------------
    [[nodiscard]] TreeMetadata const& metadata() const
    {
        return m_metadata;
    }

    // ------------------------------------------------------------------------
    //! \brief Attach the blackboard used by nodes in this tree.
    // ------------------------------------------------------------------------
    void setBlackboard(Blackboard::Ptr p_blackboard)
    {
        m_blackboard = std::move(p_blackboard);
    }

    // ------------------------------------------------------------------------
    //! \brief Return the blackboard attached to this tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Blackboard::Ptr blackboard() const
    {
        return m_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Configure output port remapping toward a parent blackboard.
    // ------------------------------------------------------------------------
    void setOutputRemapping(
        std::unordered_map<std::string, std::string> const& p_remapping)
    {
        m_outputRemapping = p_remapping;
    }

    // ------------------------------------------------------------------------
    //! \brief Set the parent blackboard receiving remapped outputs.
    // ------------------------------------------------------------------------
    void setParentBlackboard(Blackboard::Ptr p_parent)
    {
        m_parentBlackboard = std::move(p_parent);
    }

    // ------------------------------------------------------------------------
    //! \brief Copy remapped outputs from this tree to its parent blackboard.
    // ------------------------------------------------------------------------
    void propagateOutputs() const;

    // ------------------------------------------------------------------------
    //! \brief Check whether the tree has a valid root node graph.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValid() const;

    // ------------------------------------------------------------------------
    //! \brief Execute one tick starting from the root node.
    //! \return Status returned by the root node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tick();

    // ------------------------------------------------------------------------
    //! \brief Attach a client used to stream runtime state to a visualizer.
    // ------------------------------------------------------------------------
    void setVisualizerClient(std::shared_ptr<VisualizerClient> p_visualizer)
    {
        m_visualizer = std::move(p_visualizer);
    }

    // ------------------------------------------------------------------------
    //! \brief Return the attached visualizer client, if any.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::shared_ptr<VisualizerClient> visualizerClient() const
    {
        return m_visualizer;
    }

    // ------------------------------------------------------------------------
    //! \brief Reset the root node and clear the cached tree status.
    // ------------------------------------------------------------------------
    void reset();

    // ------------------------------------------------------------------------
    //! \brief Halt the root node and clear the cached tree status.
    // ------------------------------------------------------------------------
    void halt();

    // ------------------------------------------------------------------------
    //! \brief Alias for \ref halt().
    // ------------------------------------------------------------------------
    void haltTree()
    {
        halt();
    }

    // ------------------------------------------------------------------------
    //! \brief Return the status produced by the last \ref tick().
    // ------------------------------------------------------------------------
    [[nodiscard]] Status status() const
    {
        return m_status;
    }

    // ------------------------------------------------------------------------
    //! \brief Accept a const visitor on this tree and its nodes.
    // ------------------------------------------------------------------------
    void accept(ConstBehaviorTreeVisitor& p_visitor) const;

    // ------------------------------------------------------------------------
    //! \brief Accept a visitor on this tree and its nodes.
    // ------------------------------------------------------------------------
    void accept(BehaviorTreeVisitor& p_visitor);

    // ------------------------------------------------------------------------
    //! \brief Find a SubTreeNode by name in this tree and nested subtrees.
    // ------------------------------------------------------------------------
    [[nodiscard]] SubTreeNode* findSubTree(std::string const& p_name);

    // ------------------------------------------------------------------------
    //! \brief Find a SubTreeNode by name in this tree and nested subtrees.
    // ------------------------------------------------------------------------
    [[nodiscard]] SubTreeNode const*
    findSubTree(std::string const& p_name) const;

    // ------------------------------------------------------------------------
    //! \brief Check whether a node slot forms a valid subtree.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isNodeValid(std::size_t p_index) const;

    // ------------------------------------------------------------------------
    //! \brief Execute one node tick (setup, run, teardown).
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tickNode(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Reset one node and its descendants.
    // ------------------------------------------------------------------------
    void resetNode(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Halt one node and its descendants.
    // ------------------------------------------------------------------------
    void haltNode(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Return the SoA runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] Runtime& runtime()
    {
        return m_runtime;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the SoA runtime state.
    // ------------------------------------------------------------------------
    [[nodiscard]] Runtime const& runtime() const
    {
        return m_runtime;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the node kind for a storage slot.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeKind kind(std::size_t p_index) const
    {
        return m_kinds[p_index];
    }

    // ------------------------------------------------------------------------
    //! \brief Return mutable node configuration for a storage slot.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeConfig& config(std::size_t p_index)
    {
        return m_configs[p_index];
    }

    // ------------------------------------------------------------------------
    //! \brief Return node configuration for a storage slot.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeConfig const& config(std::size_t p_index) const
    {
        return m_configs[p_index];
    }

private:

    // ------------------------------------------------------------------------
    //! \brief Setup phase executed before the first run tick of a node.
    //! \param[in] p_index Storage index of the node.
    //! \return Initial status for the run phase.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status setUpNode(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Run phase implementing the node kind behavior.
    //! \param[in] p_index Storage index of the node.
    //! \return Status returned by the node logic.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status runNode(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Tear-down phase executed when a node leaves RUNNING.
    //! \param[in] p_index Storage index of the node.
    //! \param[in] p_status Final status produced by the run phase.
    // ------------------------------------------------------------------------
    void tearDownNode(std::size_t p_index, Status p_status);

    // ------------------------------------------------------------------------
    //! \brief Halt a running node and propagate to descendants.
    //! \param[in] p_index Storage index of the node.
    // ------------------------------------------------------------------------
    void haltNodeInternal(std::size_t p_index);

    // ------------------------------------------------------------------------
    //! \brief Return the storage index of a decorator child.
    //! \param[in] p_index Storage index of the decorator node.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t decoratorChildIndex(std::size_t p_index) const;

    // ------------------------------------------------------------------------
    //! \brief Tick the single child of a decorator node.
    //! \param[in] p_index Storage index of the decorator node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tickDecoratorChild(std::size_t p_index);

    //! \brief Object pool storing all nodes of this tree.
    Node::Pool m_pool;
    //! \brief Node kinds parallel to pool slots.
    std::vector<NodeKind> m_kinds;
    //! \brief Node configuration parallel to pool slots.
    std::vector<NodeConfig> m_configs;
    //! \brief SoA runtime state parallel to pool slots.
    Runtime m_runtime;
    //! \brief Storage index of the node executed by tick().
    size_t m_root_index = INVALID_NODE_INDEX;
    //! \brief Status returned by the last tick().
    Status m_status = Status::INVALID;
    //! \brief Visualizer IDs parallel to pool slots.
    TreeMetadata m_metadata;
    //! \brief Shared blackboard for nodes in this tree.
    Blackboard::Ptr m_blackboard = nullptr;
    //! \brief Optional client streaming runtime state to a visualizer.
    std::shared_ptr<VisualizerClient> m_visualizer = nullptr;
    //! \brief Output port remapping toward a parent blackboard.
    std::unordered_map<std::string, std::string> m_outputRemapping;
    //! \brief Parent blackboard receiving remapped outputs.
    Blackboard::Ptr m_parentBlackboard = nullptr;
};

} // namespace bt
