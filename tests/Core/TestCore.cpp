/**
 * @file TestCore.cpp
 * @brief Unit tests for Core components: Node, Tree, Status, and lifecycle.
 *
 * Corresponds to src/BlackThorn/Core/
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// Helper Classes for Testing
// ===========================================================================

namespace {

// ****************************************************************************
//! \brief Action node that executes a lambda function for testing purposes.
//! \details Allows flexible testing by accepting custom tick and reset
//!          functions via lambdas.
// ****************************************************************************
class LambdaTestAction: public bt::Action
{
public:

    using Tick = std::function<bt::Status()>;
    using Reset = std::function<void()>;

    // ------------------------------------------------------------------------
    //! \brief Create a lambda-based action with custom tick and reset handlers.
    //! \param[in] tick The function to execute on tick.
    //! \param[in] reset The function to execute on reset (optional).
    // ------------------------------------------------------------------------
    explicit LambdaTestAction(Tick tick, Reset reset = {})
        : m_tick(std::move(tick)), m_reset(std::move(reset))
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Create a lambda-based action from a pair of handlers.
    //! \param[in] handlers A pair containing tick and reset handlers.
    // ------------------------------------------------------------------------
    explicit LambdaTestAction(std::pair<Tick, Reset> handlers)
        : LambdaTestAction(std::move(handlers.first),
                           std::move(handlers.second))
    {
    }

    bt::Status onRunning() override
    {
        return m_tick ? m_tick() : bt::Status::FAILURE;
    }

    void reset() override
    {
        bt::Action::reset();
        if (m_reset)
        {
            m_reset();
        }
    }

private:

    Tick m_tick;
    Reset m_reset;
};

// ****************************************************************************
//! \brief Action node that increments a counter for testing purposes.
//! \details Used to verify execution order and count how many times a node
//!          has been executed.
// ****************************************************************************
class CounterAction final: public LambdaTestAction
{
public:

    // ------------------------------------------------------------------------
    //! \brief Create a counter action that increments the given pointer.
    //! \param[in] counter Pointer to the integer counter to increment.
    // ------------------------------------------------------------------------
    explicit CounterAction(int* counter)
        : LambdaTestAction([counter]() {
              (*counter)++;
              return bt::Status::SUCCESS;
          })
    {
    }
};

// ****************************************************************************
//! \brief Action node that returns a specific status for testing purposes.
//! \details Allows testing behavior with predetermined status values.
// ****************************************************************************
class StatusAction final: public LambdaTestAction
{
public:

    // ------------------------------------------------------------------------
    //! \brief Create a status action that always returns the given status.
    //! \param[in] status The status value to return on each tick.
    // ------------------------------------------------------------------------
    explicit StatusAction(bt::Status status)
        : LambdaTestAction([status]() { return status; })
    {
    }
};

} // anonymous namespace

// ===========================================================================
// Status to String Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test status to string conversion.
//! \details GIVEN different status values, WHEN converting to string,
//!          THEN EXPECT the correct string representation.
// ------------------------------------------------------------------------
TEST(TestStatus, StatusToString)
{
    // GIVEN: Status enum values
    // WHEN: Converting each status to string
    // THEN: EXPECT correct string representation
    EXPECT_EQ(to_string(bt::Status::INVALID), "INVALID");
    EXPECT_EQ(to_string(bt::Status::RUNNING), "RUNNING");
    EXPECT_EQ(to_string(bt::Status::SUCCESS), "SUCCESS");
    EXPECT_EQ(to_string(bt::Status::FAILURE), "FAILURE");
}

// ===========================================================================
// Tree Lifecycle Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test tree creation and root node assignment.
//! \details GIVEN a newly created tree, WHEN checking for root node,
//!          THEN EXPECT no root exists. WHEN setting a root node,
//!          THEN EXPECT the tree has a root and is valid.
// ------------------------------------------------------------------------
TEST(TestTree, CreateAndSetRoot)
{
    // GIVEN: A newly created tree
    auto tree = bt::Tree::create();

    // WHEN: Checking for root node
    // THEN: EXPECT no root exists and tree is invalid
    EXPECT_FALSE(tree->hasRoot());
    EXPECT_FALSE(tree->isValid());

    // WHEN: Setting a root node
    tree->setRoot(bt::Node::create<bt::Success>());

    // THEN: EXPECT the tree has a root and is valid
    EXPECT_TRUE(tree->hasRoot());
    EXPECT_TRUE(tree->isValid());
}

// ------------------------------------------------------------------------
//! \brief Test direct root node creation.
//! \details GIVEN a newly created tree, WHEN creating root directly,
//!          THEN EXPECT the tree has a root node.
// ------------------------------------------------------------------------
TEST(TestTree, CreateRootDirectly)
{
    // GIVEN: A newly created tree
    auto tree = bt::Tree::create();

    // WHEN: Creating root node directly
    [[maybe_unused]] auto& seq = tree->createRoot<bt::Sequence>();

    // THEN: EXPECT the tree has a root
    EXPECT_TRUE(tree->hasRoot());
}

// ------------------------------------------------------------------------
//! \brief Test tree execution with a sequence of success nodes.
//! \details GIVEN a tree with a sequence containing success children,
//!          WHEN ticking the tree, THEN EXPECT the tree is valid and
//!          returns SUCCESS status.
// ------------------------------------------------------------------------
TEST(TestTree, TickTree)
{
    // GIVEN: A tree with a sequence containing success children
    auto tree = bt::Tree::create();
    auto& seq = tree->createRoot<bt::Sequence>();
    seq.addChild(bt::Node::create<bt::Success>());
    seq.addChild(bt::Node::create<bt::Success>());

    // WHEN: Ticking the tree
    // THEN: EXPECT the tree is valid and returns SUCCESS
    EXPECT_TRUE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ------------------------------------------------------------------------
//! \brief Test tree without root node.
//! \details GIVEN a tree without a root node, WHEN checking validity
//!          or ticking, THEN EXPECT the tree is invalid and returns
//!          FAILURE status.
// ------------------------------------------------------------------------
TEST(TestTree, InvalidTreeWithoutRoot)
{
    // GIVEN: A tree without a root node
    auto tree = bt::Tree::create();

    // WHEN: Checking validity or ticking
    // THEN: EXPECT the tree is invalid and returns FAILURE
    EXPECT_FALSE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

// ------------------------------------------------------------------------
//! \brief Test tree with invalid children (empty sequence).
//! \details GIVEN a tree with an empty sequence as root, WHEN checking
//!          validity or ticking, THEN EXPECT the tree is invalid and
//!          returns FAILURE status.
// ------------------------------------------------------------------------
TEST(TestTree, InvalidTreeWithInvalidChildren)
{
    // GIVEN: A tree with an empty sequence (which is invalid)
    auto tree = bt::Tree::create();
    [[maybe_unused]] auto& seq = tree->createRoot<bt::Sequence>();

    // WHEN: Checking validity or ticking
    // THEN: EXPECT the tree is invalid and returns FAILURE
    EXPECT_FALSE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

// ------------------------------------------------------------------------
//! \brief Test tree reset functionality.
//! \details GIVEN a tree that has been executed successfully, WHEN
//!          resetting the tree, THEN EXPECT the tree status becomes
//!          INVALID.
// ------------------------------------------------------------------------
TEST(TestTree, ResetTree)
{
    // GIVEN: A tree that has been executed successfully
    auto tree = bt::Tree::create();
    auto& seq = tree->createRoot<bt::Sequence>();
    seq.addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(tree->status(), bt::Status::SUCCESS);

    // WHEN: Resetting the tree
    tree->reset();

    // THEN: EXPECT the tree status becomes INVALID
    EXPECT_EQ(tree->status(), bt::Status::INVALID);
}

// ------------------------------------------------------------------------
//! \brief Test tree halt functionality.
//! \details GIVEN a running tree, WHEN halting the tree, THEN EXPECT
//!          the tree status becomes INVALID.
// ------------------------------------------------------------------------
TEST(TestTree, HaltTree)
{
    // GIVEN: A tree with a running child node
    auto tree = bt::Tree::create();
    auto& seq = tree->createRoot<bt::Sequence>();
    seq.addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
    EXPECT_EQ(tree->status(), bt::Status::RUNNING);

    // WHEN: Halting the tree
    tree->haltTree();

    // THEN: EXPECT the tree status becomes INVALID
    EXPECT_EQ(tree->status(), bt::Status::INVALID);
}

// ------------------------------------------------------------------------
//! \brief Test tree with blackboard integration.
//! \details GIVEN a blackboard with data, WHEN setting it to a tree,
//!          THEN EXPECT the tree can access the blackboard and its data.
// ------------------------------------------------------------------------
TEST(TestTree, TreeWithBlackboard)
{
    // GIVEN: A blackboard with data
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value", 42);

    // WHEN: Setting the blackboard to a tree
    auto tree = bt::Tree::create();
    tree->setBlackboard(bb);

    // THEN: EXPECT the tree can access the blackboard and its data
    EXPECT_EQ(tree->blackboard(), bb);
    EXPECT_EQ(tree->blackboard()->get<int>("value"), 42);
}

// ===========================================================================
// Node Reset and Halt Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test node reset functionality.
//! \details GIVEN a node that has been executed, WHEN resetting the node,
//!          THEN EXPECT the node status becomes INVALID.
// ------------------------------------------------------------------------
TEST(TestNodeLifecycle, ResetNode)
{
    // GIVEN: A node that has been executed
    auto node = bt::Node::create<bt::Success>();
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(node->status(), bt::Status::SUCCESS);

    // WHEN: Resetting the node
    node->reset();

    // THEN: EXPECT the node status becomes INVALID
    EXPECT_EQ(node->status(), bt::Status::INVALID);
}

// ------------------------------------------------------------------------
//! \brief Test halting a running node.
//! \details GIVEN a running node, WHEN halting the node, THEN EXPECT
//!          the node status becomes INVALID.
// ------------------------------------------------------------------------
TEST(TestNodeLifecycle, HaltRunningNode)
{
    // GIVEN: A running node
    auto node = bt::Node::create<StatusAction>(bt::Status::RUNNING);
    EXPECT_EQ(node->tick(), bt::Status::RUNNING);
    EXPECT_EQ(node->status(), bt::Status::RUNNING);

    // WHEN: Halting the node
    node->halt();

    // THEN: EXPECT the node status becomes INVALID
    EXPECT_EQ(node->status(), bt::Status::INVALID);
}

// ------------------------------------------------------------------------
//! \brief Test halting a non-running node.
//! \details GIVEN a node that has completed (not running), WHEN halting
//!          the node, THEN EXPECT the node status becomes INVALID.
// ------------------------------------------------------------------------
TEST(TestNodeLifecycle, HaltNonRunningNode)
{
    // GIVEN: A node that has completed (not running)
    auto node = bt::Node::create<bt::Success>();
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(node->status(), bt::Status::SUCCESS);

    // WHEN: Halting the node
    node->halt();

    // THEN: EXPECT the node status becomes INVALID
    EXPECT_EQ(node->status(), bt::Status::INVALID);
}

// ------------------------------------------------------------------------
//! \brief Test resetting a composite node.
//! \details GIVEN a composite node that has been executed, WHEN resetting
//!          the composite, THEN EXPECT the composite and its children are
//!          reset and can be executed again.
// ------------------------------------------------------------------------
TEST(TestNodeLifecycle, ResetComposite)
{
    // GIVEN: A composite node that has been executed
    auto seq = bt::Node::create<bt::Sequence>();
    seq->addChild(bt::Node::create<bt::Success>());
    seq->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(seq->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(seq->status(), bt::Status::SUCCESS);

    // WHEN: Resetting the composite
    seq->reset();
    EXPECT_EQ(seq->status(), bt::Status::INVALID);

    // THEN: EXPECT children are also reset and can be executed again
    EXPECT_EQ(seq->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(seq->status(), bt::Status::SUCCESS);
}

// ------------------------------------------------------------------------
//! \brief Test halting a composite node.
//! \details GIVEN a running composite node, WHEN halting the composite,
//!          THEN EXPECT the composite status becomes INVALID.
// ------------------------------------------------------------------------
TEST(TestNodeLifecycle, HaltComposite)
{
    // GIVEN: A running composite node
    auto seq = bt::Node::create<bt::Sequence>();
    seq->addChild(bt::Node::create<bt::Success>());
    seq->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(seq->tick(), bt::Status::RUNNING);
    EXPECT_EQ(seq->status(), bt::Status::RUNNING);

    // WHEN: Halting the composite
    seq->halt();

    // THEN: EXPECT the composite status becomes INVALID
    EXPECT_EQ(seq->status(), bt::Status::INVALID);
}
