/**
 * @file TestSequences.cpp
 * @brief Unit tests for Sequence composite nodes.
 *
 * Corresponds to src/BlackThorn/Nodes/Composites/Sequences.hpp
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
// Sequence Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test sequence with all children succeeding.
//! \details GIVEN a sequence with all success children, WHEN ticking
//!          the sequence, THEN EXPECT the sequence is valid and returns
//!          SUCCESS status.
// ------------------------------------------------------------------------
TEST(TestSequence, AllChildrenSuccess)
{
    // GIVEN: A sequence with all success children
    auto sequence = bt::Node::create<bt::Sequence>();
    sequence->addChild(bt::Node::create<bt::Success>());
    sequence->addChild(bt::Node::create<bt::Success>());
    sequence->addChild(bt::Node::create<bt::Success>());

    // WHEN: Ticking the sequence
    // THEN: EXPECT the sequence is valid and returns SUCCESS
    EXPECT_TRUE(sequence->isValid());
    EXPECT_EQ(sequence->tick(), bt::Status::SUCCESS);
}

TEST(TestSequence, FirstChildFails)
{
    auto sequence = bt::Node::create<bt::Sequence>();
    sequence->addChild(bt::Node::create<bt::Failure>());
    sequence->addChild(bt::Node::create<bt::Success>());
    sequence->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(sequence->tick(), bt::Status::FAILURE);
}

TEST(TestSequence, MiddleChildFails)
{
    int counter = 0;
    auto sequence = bt::Node::create<bt::Sequence>();
    sequence->addChild(bt::Node::create<CounterAction>(&counter));
    sequence->addChild(bt::Node::create<bt::Failure>());
    sequence->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(sequence->tick(), bt::Status::FAILURE);
    EXPECT_EQ(counter, 1); // Only first child executed
}

TEST(TestSequence, ChildReturnsRunning)
{
    auto sequence = bt::Node::create<bt::Sequence>();
    sequence->addChild(bt::Node::create<bt::Success>());
    sequence->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));
    sequence->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);

    // On next tick, should resume from RUNNING child
    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);
}

TEST(TestSequence, EmptySequenceFails)
{
    auto sequence = bt::Node::create<bt::Sequence>();
    EXPECT_FALSE(sequence->isValid());
}

TEST(TestSequence, ExecutionOrder)
{
    std::vector<int> execution_order;
    auto sequence = bt::Node::create<bt::Sequence>();

    sequence->addChild(bt::Node::create<bt::SugarAction>([&execution_order]() {
        execution_order.push_back(1);
        return bt::Status::SUCCESS;
    }));
    sequence->addChild(bt::Node::create<bt::SugarAction>([&execution_order]() {
        execution_order.push_back(2);
        return bt::Status::SUCCESS;
    }));
    sequence->addChild(bt::Node::create<bt::SugarAction>([&execution_order]() {
        execution_order.push_back(3);
        return bt::Status::SUCCESS;
    }));

    EXPECT_EQ(sequence->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

// ===========================================================================
// ReactiveSequence Tests
// ===========================================================================

TEST(TestReactiveSequence, RestartsOnEachTick)
{
    int counter = 0;
    auto sequence = bt::Node::create<bt::ReactiveSequence>();
    sequence->addChild(bt::Node::create<CounterAction>(&counter));
    sequence->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 1);

    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 2); // First child executed again
}

TEST(TestReactiveSequence, AllSuccess)
{
    auto sequence = bt::Node::create<bt::ReactiveSequence>();
    sequence->addChild(bt::Node::create<bt::Success>());
    sequence->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(sequence->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// SequenceWithMemory Tests
// ===========================================================================

TEST(TestSequenceWithMemory, RemembersProgress)
{
    int counter = 0;
    auto sequence = bt::Node::create<bt::SequenceWithMemory>();
    sequence->addChild(bt::Node::create<CounterAction>(&counter));
    sequence->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));
    sequence->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 1); // First child executed

    EXPECT_EQ(sequence->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 1); // First child NOT executed again
}
