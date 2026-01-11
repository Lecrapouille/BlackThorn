/**
 * @file Tree.hpp
 * @brief Tree container, SubTreeHandle, and SubTreeNode classes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Core/Node.hpp"

#include <cassert>
#include <memory>

namespace bt {

// Forward declarations
class VisualizerClient;

// ****************************************************************************
//! \brief Container for a behavior tree instance.
//! \details Unlike the original BrainTree implementation, Tree is no longer a
//!          Node itself. Instead, it simply owns a root node and a blackboard,
//!          making it easier to embed trees as reusable subtrees.
// ****************************************************************************
class Tree
{
public:

    using Ptr = std::unique_ptr<Tree>;

    // ------------------------------------------------------------------------
    //! \brief Create a new tree.
    //! \return A unique pointer to the new tree.
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
    //! \brief Check if the tree has a root node.
    //! \return True if the tree has a root node, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline bool hasRoot() const
    {
        return m_root != nullptr;
    }

    // ------------------------------------------------------------------------
    //! \brief Create and set the root node of the behavior tree.
    //! \param[in] p_args The arguments to pass to the constructor of T.
    //! \return A reference to the new root node.
    // ------------------------------------------------------------------------
    template <class T, typename... Args>
    [[nodiscard]] inline T& createRoot(Args&&... p_args)
    {
        static_assert(std::is_base_of_v<Node, T>, "T must inherit from Node");
        auto root = Node::create<T>(std::forward<Args>(p_args)...);
        T* ptr = root.get();
        m_root = std::move(root);
        return *ptr;
    }

    // ------------------------------------------------------------------------
    //! \brief Set the root node of the behavior tree.
    //! \param[in] p_root Pointer to the root node.
    // ------------------------------------------------------------------------
    void setRoot(Node::Ptr p_root)
    {
        m_root = std::move(p_root);
    }

    // ------------------------------------------------------------------------
    //! \brief Get the root node of the behavior tree (const version).
    //! \return Const reference to the root node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node const& getRoot() const
    {
        assert(m_root != nullptr && "Root node is not set");
        return *m_root;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the root node of the behavior tree (non-const version).
    //! \return Reference to the root node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node& getRoot()
    {
        assert(m_root != nullptr && "Root node is not set");
        return *m_root;
    }

    // ------------------------------------------------------------------------
    //! \brief Set the blackboard of the tree
    //! \param[in] p_blackboard The blackboard to set
    // ------------------------------------------------------------------------
    void setBlackboard(Blackboard::Ptr p_blackboard)
    {
        m_blackboard = std::move(p_blackboard);
    }

    // ------------------------------------------------------------------------
    //! \brief Get the blackboard of the tree
    //! \return A pointer to the blackboard
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Blackboard::Ptr blackboard() const
    {
        return m_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if the tree is valid before starting the tree.
    //! \details The tree is valid if it has a root node and all nodes in the
    //! tree are valid (i.e. if decorators and composites have children).
    //! \return True if the tree is valid, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValid() const
    {
        return (m_root != nullptr) && (m_root->isValid());
    }

    // ------------------------------------------------------------------------
    //! \brief Execute one tick of the tree.
    //! \return The status returned by the root node.
    //! \note Defined after VisualizerClient include to resolve dependency.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status tick();

    // ------------------------------------------------------------------------
    //! \brief Attach a visualizer client for real-time tree monitoring.
    //! When attached, the tree will automatically send state changes after
    //! each tick() call.
    //! \param[in] p_visualizer The visualizer client to attach.
    // ------------------------------------------------------------------------
    void setVisualizerClient(std::shared_ptr<VisualizerClient> p_visualizer)
    {
        m_visualizer = std::move(p_visualizer);
    }

    // ------------------------------------------------------------------------
    //! \brief Get the attached visualizer client.
    //! \return The visualizer client, or nullptr if none attached.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::shared_ptr<VisualizerClient> visualizerClient() const
    {
        return m_visualizer;
    }

    // ------------------------------------------------------------------------
    //! \brief Reset the tree state and recursively reset all nodes.
    // ------------------------------------------------------------------------
    void reset()
    {
        if (m_root != nullptr)
        {
            m_root->reset();
        }
        m_status = Status::INVALID;
    }

    // ------------------------------------------------------------------------
    //! \brief Halt the entire tree recursively.
    // ------------------------------------------------------------------------
    void halt()
    {
        if (m_root != nullptr)
        {
            m_root->halt();
        }
        m_status = Status::INVALID;
    }

    // ------------------------------------------------------------------------
    //! \brief Backward compatible helper.
    // ------------------------------------------------------------------------
    void haltTree()
    {
        halt();
    }

    // ------------------------------------------------------------------------
    //! \brief Get the last known status of the tree.
    //! \return The cached execution status.
    // ------------------------------------------------------------------------
    [[nodiscard]] Status status() const
    {
        return m_status;
    }

    // ------------------------------------------------------------------------
    //! \brief Accept a const visitor (read-only).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    void accept(ConstBehaviorTreeVisitor& p_visitor) const
    {
        p_visitor.visitTree(*this);
    }

    // ------------------------------------------------------------------------
    //! \brief Accept a non-const visitor (read-write).
    //! \param[in] p_visitor The visitor to accept.
    // ------------------------------------------------------------------------
    void accept(BehaviorTreeVisitor& p_visitor)
    {
        p_visitor.visitTree(*this);
    }

private:

    //! \brief The root node of the behavior tree.
    Node::Ptr m_root = nullptr;
    //! \brief The blackboard associated with this tree.
    Blackboard::Ptr m_blackboard = nullptr;
    //! \brief Cached execution status of the tree.
    Status m_status = Status::INVALID;
    //! \brief Optional visualizer client for real-time monitoring.
    std::shared_ptr<VisualizerClient> m_visualizer = nullptr;
};

// ****************************************************************************
//! \brief Handle storing a reusable subtree instance.
// ****************************************************************************
class SubTreeHandle
{
public:

    using Ptr = std::shared_ptr<SubTreeHandle>;

    SubTreeHandle(std::string p_id, Tree::Ptr p_tree)
        : m_id(std::move(p_id)), m_tree(std::move(p_tree))
    {
    }

    [[nodiscard]] std::string const& id() const
    {
        return m_id;
    }

    [[nodiscard]] Tree& tree()
    {
        assert(m_tree != nullptr && "SubTree handle has no tree");
        return *m_tree;
    }

    [[nodiscard]] Tree const& tree() const
    {
        assert(m_tree != nullptr && "SubTree handle has no tree");
        return *m_tree;
    }

private:

    std::string m_id;
    Tree::Ptr m_tree;
};

// ****************************************************************************
//! \brief Node that executes another Tree instance as a child.
// ****************************************************************************
class SubTreeNode final: public Node
{
public:

    explicit SubTreeNode(SubTreeHandle::Ptr p_handle)
        : m_handle(std::move(p_handle))
    {
        m_type = toString();
    }

    static constexpr char const* toString()
    {
        return "SubTree";
    }

    [[nodiscard]] bool isValid() const override
    {
        return (m_handle != nullptr) && m_handle->tree().hasRoot() &&
               m_handle->tree().isValid();
    }

protected:

    [[nodiscard]] Status onSetUp() override
    {
        if (!m_handle)
        {
            return Status::FAILURE;
        }
        m_handle->tree().reset();
        return Status::RUNNING;
    }

    [[nodiscard]] Status onRunning() override
    {
        if (!m_handle)
        {
            return Status::FAILURE;
        }
        return m_handle->tree().tick();
    }

    void onTearDown(Status) override
    {
        if (m_handle)
        {
            m_handle->tree().reset();
        }
    }

    void onHalt() override
    {
        if (m_handle)
        {
            m_handle->tree().halt();
        }
    }

public:

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSubTree(*this);
    }

    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSubTree(*this);
    }

    [[nodiscard]] SubTreeHandle::Ptr handle() const
    {
        return m_handle;
    }

private:

    SubTreeHandle::Ptr m_handle;
};

} // namespace bt

// Include VisualizerClient for Tree::tick() implementation
// Must be outside namespace bt to avoid nested namespace issues.
#include "BlackThorn/Network/VisualizerClient.hpp"

namespace bt {

// ----------------------------------------------------------------------------
// Tree::tick() implementation - defined here after VisualizerClient is complete
// ----------------------------------------------------------------------------
inline Status Tree::tick()
{
    if (!m_root)
    {
        m_status = Status::FAILURE;
        return m_status;
    }
    if (!m_root->isValid())
    {
        m_status = Status::FAILURE;
        return m_status;
    }

    m_status = m_root->tick();

    // Send state changes to visualizer if connected
    if (m_visualizer && m_visualizer->isConnected())
    {
        m_visualizer->sendStateChanges(*this);
    }

    return m_status;
}

} // namespace bt
