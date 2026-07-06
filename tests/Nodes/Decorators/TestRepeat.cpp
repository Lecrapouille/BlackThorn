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

class LambdaTestAction: public bt::CallbackLeaf
{
public:

    using Tick = std::function<bt::Status()>;
    using Reset = std::function<void()>;

    explicit LambdaTestAction(Tick tick, Reset reset = {})
        : CallbackLeaf(std::move(tick), std::move(reset))
    {
    }

    explicit LambdaTestAction(std::pair<Tick, Reset> handlers)
        : CallbackLeaf(std::move(handlers.first), std::move(handlers.second))
    {
    }
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

// ---------------------------------------------------------------------------
//! \brief Stateful action that resets index on reset() - for Repeat tests.
// ---------------------------------------------------------------------------
class StatefulAction final: public LambdaTestAction
{
public:

    using Tick = LambdaTestAction::Tick;
    using Reset = LambdaTestAction::Reset;

    explicit StatefulAction(std::vector<bt::Status> statuses)
        : LambdaTestAction(makeHandlers(std::move(statuses)))
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

// ---------------------------------------------------------------------------
//! \brief Sequence action that does NOT reset index - for UntilSuccess/Failure.
//! \details This action advances through statuses regardless of reset() calls,
//!          which is needed because UntilSuccess/UntilFailure call reset()
//!          after each child completion.
// ---------------------------------------------------------------------------
class SequenceAction final: public LambdaTestAction
{
public:

    explicit SequenceAction(std::vector<bt::Status> statuses)
        : LambdaTestAction(makeHandler(std::move(statuses)))
    {
    }

private:

    static Tick makeHandler(std::vector<bt::Status> statuses)
    {
        auto state =
            std::make_shared<std::pair<std::vector<bt::Status>, size_t>>(
                std::move(statuses), 0);

        return [state]() {
            if (state->second < state->first.size())
            {
                return state->first[state->second++];
            }
            return bt::Status::SUCCESS;
        };
    }
};

} // anonymous namespace

// ===========================================================================
// Repeat Tests
// ===========================================================================

TEST(TestRepeat, RepeatExactTimes)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& repeat = tree->createRoot<bt::Repeat>(3);
    repeat.createChild<CounterAction>(&counter);

    EXPECT_EQ(repeat.tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 1);

    EXPECT_EQ(repeat.tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 2);

    EXPECT_EQ(repeat.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 3);

    EXPECT_EQ(repeat.getCount(), 3);
    EXPECT_EQ(repeat.getRepetitions(), 3);
}

TEST(TestRepeat, ChildFailureIgnored)
{
    // Repeat ignores child's FAILURE status and continues until limit reached
    auto tree = bt::Tree::create();
    auto& repeat = tree->createRoot<bt::Repeat>(3);
    repeat.createChild<StatefulAction>(std::vector<bt::Status>{
        bt::Status::SUCCESS, bt::Status::FAILURE, bt::Status::SUCCESS});

    EXPECT_EQ(repeat.tick(), bt::Status::RUNNING); // Child SUCCESS, continue
    EXPECT_EQ(repeat.tick(), bt::Status::RUNNING); // Child FAILURE, continue
    EXPECT_EQ(repeat.tick(),
              bt::Status::SUCCESS); // Child SUCCESS, limit reached
}

TEST(TestRepeat, ChildRunningPropagates)
{
    auto tree = bt::Tree::create();
    auto& repeat = tree->createRoot<bt::Repeat>(3);
    repeat.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(repeat.tick(), bt::Status::RUNNING);
}

TEST(TestRepeat, InfiniteRepeat)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& repeat = tree->createRoot<bt::Repeat>(0); // 0 means infinite
    repeat.createChild<CounterAction>(&counter);

    // Should keep running forever
    for (int i = 0; i < 100; i++)
    {
        EXPECT_EQ(repeat.tick(), bt::Status::RUNNING);
    }
    EXPECT_EQ(counter, 100);
}

// ===========================================================================
// UntilSuccess Tests
// ===========================================================================

TEST(TestUntilSuccess, ImmediateSuccess)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>();
    until.createChild<bt::Success>();

    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
}

TEST(TestUntilSuccess, SuccessOnFirstTry)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>(3);
    until.createChild<CounterAction>(&counter);

    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
}

// ---------------------------------------------------------------------------
//! \brief Test UntilSuccess retries on child failure.
//! \details GIVEN an UntilSuccess decorator with a child that fails twice
//!          then succeeds, WHEN ticking the decorator, THEN it returns
//!          RUNNING while child fails and SUCCESS when child succeeds.
// ---------------------------------------------------------------------------
TEST(TestUntilSuccess, RetryOnFailure)
{
    // GIVEN: UntilSuccess with child returning FAILURE, FAILURE, SUCCESS
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>(3);
    until.createChild<SequenceAction>(std::vector<bt::Status>{
        bt::Status::FAILURE, bt::Status::FAILURE, bt::Status::SUCCESS});

    // WHEN/THEN: First two ticks return RUNNING (child failed, retry)
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);

    // WHEN/THEN: Third tick returns SUCCESS (child finally succeeded)
    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
}

TEST(TestUntilSuccess, MaxAttemptsReached)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>(2);
    until.createChild<bt::Failure>();

    EXPECT_EQ(until.tick(), bt::Status::RUNNING); // First attempt
    EXPECT_EQ(until.tick(), bt::Status::FAILURE); // Max attempts reached

    EXPECT_EQ(until.getCount(), 2);
    EXPECT_EQ(until.getAttempts(), 2);
}

TEST(TestUntilSuccess, RunningPropagates)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>(3);
    until.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
}

// ---------------------------------------------------------------------------
//! \brief Test UntilSuccess with infinite attempts.
//! \details GIVEN an UntilSuccess decorator with infinite attempts (0) and
//!          a child that fails multiple times then succeeds, WHEN ticking,
//!          THEN it keeps retrying until child succeeds.
// ---------------------------------------------------------------------------
TEST(TestUntilSuccess, InfiniteAttempts)
{
    // GIVEN: UntilSuccess with infinite attempts and child failing 3 times
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilSuccess>(0);
    until.createChild<SequenceAction>(
        std::vector<bt::Status>{bt::Status::FAILURE,
                                bt::Status::FAILURE,
                                bt::Status::FAILURE,
                                bt::Status::SUCCESS});

    // WHEN/THEN: Keeps returning RUNNING while child fails
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);

    // WHEN/THEN: Returns SUCCESS when child finally succeeds
    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(until.getAttempts(), 0);
}

// ===========================================================================
// UntilFailure Tests
// ===========================================================================

TEST(TestUntilFailure, ImmediateFailure)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>();
    until.createChild<bt::Failure>();

    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
}

TEST(TestUntilFailure, FailureOnFirstTry)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>(3);
    until.createChild<bt::Failure>();

    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
}

// ---------------------------------------------------------------------------
//! \brief Test UntilFailure retries on child success.
//! \details GIVEN an UntilFailure decorator with a child that succeeds twice
//!          then fails, WHEN ticking the decorator, THEN it returns RUNNING
//!          while child succeeds and SUCCESS when child fails.
// ---------------------------------------------------------------------------
TEST(TestUntilFailure, RetryOnSuccess)
{
    // GIVEN: UntilFailure with child returning SUCCESS, SUCCESS, FAILURE
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>(3);
    until.createChild<SequenceAction>(std::vector<bt::Status>{
        bt::Status::SUCCESS, bt::Status::SUCCESS, bt::Status::FAILURE});

    // WHEN/THEN: First two ticks return RUNNING (child succeeded, retry)
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);

    // WHEN/THEN: Third tick returns SUCCESS (child finally failed)
    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
}

TEST(TestUntilFailure, MaxAttemptsReached)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>(2);
    until.createChild<bt::Success>();

    EXPECT_EQ(until.tick(), bt::Status::RUNNING); // First attempt (success)
    EXPECT_EQ(until.tick(), bt::Status::FAILURE); // Max attempts reached

    EXPECT_EQ(until.getCount(), 2);
    EXPECT_EQ(until.getAttempts(), 2);
}

TEST(TestUntilFailure, RunningPropagates)
{
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>(3);
    until.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
}

// ---------------------------------------------------------------------------
//! \brief Test UntilFailure with infinite attempts.
//! \details GIVEN an UntilFailure decorator with infinite attempts (0) and
//!          a child that succeeds multiple times then fails, WHEN ticking,
//!          THEN it keeps retrying until child fails.
// ---------------------------------------------------------------------------
TEST(TestUntilFailure, InfiniteAttempts)
{
    // GIVEN: UntilFailure with infinite attempts and child succeeding 3 times
    auto tree = bt::Tree::create();
    auto& until = tree->createRoot<bt::UntilFailure>(0);
    until.createChild<SequenceAction>(
        std::vector<bt::Status>{bt::Status::SUCCESS,
                                bt::Status::SUCCESS,
                                bt::Status::SUCCESS,
                                bt::Status::FAILURE});

    // WHEN/THEN: Keeps returning RUNNING while child succeeds
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);
    EXPECT_EQ(until.tick(), bt::Status::RUNNING);

    // WHEN/THEN: Returns SUCCESS when child finally fails
    EXPECT_EQ(until.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(until.getAttempts(), 0);
}
