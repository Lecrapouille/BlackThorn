/**
 * @file TestRepeat.cpp
 * @brief Unit tests for Repeat decorator nodes: Repeat, UntilSuccess,
 * UntilFailure.
 *
 * Corresponds to src/BlackThorn/Nodes/Decorators/Repeat.hpp
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

class LambdaTestAction: public bt::Action
{
public:

    using Tick = std::function<bt::Status()>;
    using Reset = std::function<void()>;

    explicit LambdaTestAction(Tick tick, Reset reset = {})
        : m_tick(std::move(tick)), m_reset(std::move(reset))
    {
    }

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

class CounterAction final: public LambdaTestAction
{
public:

    explicit CounterAction(int* counter)
        : LambdaTestAction([counter]() {
              (*counter)++;
              return bt::Status::SUCCESS;
          })
    {
    }
};

class StatusAction final: public LambdaTestAction
{
public:

    explicit StatusAction(bt::Status status)
        : LambdaTestAction([status]() { return status; })
    {
    }
};

class StatefulAction final: public LambdaTestAction
{
public:

    using Tick = LambdaTestAction::Tick;
    using Reset = LambdaTestAction::Reset;

    explicit StatefulAction(std::vector<bt::Status> statuses)
        : LambdaTestAction(makeHandlers(std::move(statuses)))
    {
    }

    StatefulAction(std::initializer_list<bt::Status> statuses)
        : StatefulAction(std::vector<bt::Status>(statuses))
    {
    }

private:

    struct SharedState
    {
        std::vector<bt::Status> statuses;
        size_t index = 0;
    };

    static std::pair<Tick, Reset> makeHandlers(std::vector<bt::Status> statuses)
    {
        auto state =
            std::make_shared<SharedState>(SharedState{std::move(statuses), 0});

        Tick tick = [state]() {
            if (state->index < state->statuses.size())
            {
                return state->statuses[state->index++];
            }
            return bt::Status::SUCCESS;
        };

        Reset reset = [state]() { state->index = 0; };

        return {std::move(tick), std::move(reset)};
    }
};

} // anonymous namespace

// ===========================================================================
// Repeat Tests
// ===========================================================================

TEST(TestRepeat, RepeatExactTimes)
{
    int counter = 0;
    auto repeat = bt::Node::create<bt::Repeat>(3);
    repeat->setChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(repeat->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 1);

    EXPECT_EQ(repeat->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 2);

    EXPECT_EQ(repeat->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 3);

    EXPECT_EQ(repeat->getCount(), 3);
    EXPECT_EQ(repeat->getRepetitions(), 3);
}

TEST(TestRepeat, ChildFailureStopsRepeat)
{
    int counter = 0;
    auto repeat = bt::Node::create<bt::Repeat>(5);
    auto stateful = new StatefulAction(
        {bt::Status::SUCCESS, bt::Status::FAILURE, bt::Status::SUCCESS});
    repeat->setChild(std::unique_ptr<bt::Node>(stateful));

    EXPECT_EQ(repeat->tick(), bt::Status::RUNNING);
    EXPECT_EQ(repeat->tick(), bt::Status::FAILURE);
}

TEST(TestRepeat, ChildRunningPropagates)
{
    auto repeat = bt::Node::create<bt::Repeat>(3);
    repeat->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(repeat->tick(), bt::Status::RUNNING);
}

TEST(TestRepeat, InfiniteRepeat)
{
    int counter = 0;
    auto repeat = bt::Node::create<bt::Repeat>(0); // 0 means infinite
    repeat->setChild(bt::Node::create<CounterAction>(&counter));

    // Should keep running forever
    for (int i = 0; i < 100; i++)
    {
        EXPECT_EQ(repeat->tick(), bt::Status::RUNNING);
    }
    EXPECT_EQ(counter, 100);
}

// ===========================================================================
// UntilSuccess Tests
// ===========================================================================

TEST(TestUntilSuccess, ImmediateSuccess)
{
    auto until = bt::Node::create<bt::UntilSuccess>();
    until->setChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(until->tick(), bt::Status::SUCCESS);
}

TEST(TestUntilSuccess, SuccessOnFirstTry)
{
    int counter = 0;
    auto until = bt::Node::create<bt::UntilSuccess>(3);
    until->setChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(until->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
}

TEST(TestUntilSuccess, RetryOnFailure)
{
    auto until = bt::Node::create<bt::UntilSuccess>(3);
    auto stateful = new StatefulAction(
        {bt::Status::FAILURE, bt::Status::FAILURE, bt::Status::SUCCESS});
    until->setChild(std::unique_ptr<bt::Node>(stateful));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First failure
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Second failure
    EXPECT_EQ(until->tick(), bt::Status::SUCCESS); // Finally success
}

TEST(TestUntilSuccess, MaxAttemptsReached)
{
    auto until = bt::Node::create<bt::UntilSuccess>(2);
    until->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First attempt
    EXPECT_EQ(until->tick(), bt::Status::FAILURE); // Max attempts reached

    EXPECT_EQ(until->getCount(), 2);
    EXPECT_EQ(until->getAttempts(), 2);
}

TEST(TestUntilSuccess, RunningPropagates)
{
    auto until = bt::Node::create<bt::UntilSuccess>(3);
    until->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING);
}

TEST(TestUntilSuccess, InfiniteAttempts)
{
    auto until = bt::Node::create<bt::UntilSuccess>(0); // 0 = infinite
    auto stateful = new StatefulAction({bt::Status::FAILURE,
                                        bt::Status::FAILURE,
                                        bt::Status::FAILURE,
                                        bt::Status::SUCCESS});
    until->setChild(std::unique_ptr<bt::Node>(stateful));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First failure
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Second failure
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Third failure
    EXPECT_EQ(until->tick(), bt::Status::SUCCESS); // Finally success
    EXPECT_EQ(until->getAttempts(), 0);
}

// ===========================================================================
// UntilFailure Tests
// ===========================================================================

TEST(TestUntilFailure, ImmediateFailure)
{
    auto until = bt::Node::create<bt::UntilFailure>();
    until->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(until->tick(), bt::Status::SUCCESS);
}

TEST(TestUntilFailure, FailureOnFirstTry)
{
    auto until = bt::Node::create<bt::UntilFailure>(3);
    until->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(until->tick(), bt::Status::SUCCESS);
}

TEST(TestUntilFailure, RetryOnSuccess)
{
    auto until = bt::Node::create<bt::UntilFailure>(3);
    auto stateful = new StatefulAction(
        {bt::Status::SUCCESS, bt::Status::SUCCESS, bt::Status::FAILURE});
    until->setChild(std::unique_ptr<bt::Node>(stateful));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First success
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Second success
    EXPECT_EQ(until->tick(),
              bt::Status::SUCCESS); // Finally failure -> returns SUCCESS
}

TEST(TestUntilFailure, MaxAttemptsReached)
{
    auto until = bt::Node::create<bt::UntilFailure>(2);
    until->setChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First attempt (success)
    EXPECT_EQ(until->tick(), bt::Status::FAILURE); // Max attempts reached

    EXPECT_EQ(until->getCount(), 2);
    EXPECT_EQ(until->getAttempts(), 2);
}

TEST(TestUntilFailure, RunningPropagates)
{
    auto until = bt::Node::create<bt::UntilFailure>(3);
    until->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING);
}

TEST(TestUntilFailure, InfiniteAttempts)
{
    auto until = bt::Node::create<bt::UntilFailure>(0); // 0 = infinite
    auto stateful = new StatefulAction({bt::Status::SUCCESS,
                                        bt::Status::SUCCESS,
                                        bt::Status::SUCCESS,
                                        bt::Status::FAILURE});
    until->setChild(std::unique_ptr<bt::Node>(stateful));

    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // First success
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Second success
    EXPECT_EQ(until->tick(), bt::Status::RUNNING); // Third success
    EXPECT_EQ(until->tick(),
              bt::Status::SUCCESS); // Finally failure -> returns SUCCESS
    EXPECT_EQ(until->getAttempts(), 0);
}
