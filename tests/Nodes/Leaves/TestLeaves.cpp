/**
 * @file TestLeaves.cpp
 * @brief Unit tests for Leaf nodes: Action, Basic, Condition.
 *
 * Corresponds to src/BlackThorn/Nodes/Leaves/
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// Leaf Node Tests (Basic.hpp)
// ===========================================================================

TEST(TestLeafNodes, SuccessNode)
{
    auto node = bt::Node::create<bt::Success>();
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, FailureNode)
{
    auto node = bt::Node::create<bt::Failure>();
    EXPECT_EQ(node->tick(), bt::Status::FAILURE);
    EXPECT_EQ(node->tick(), bt::Status::FAILURE);
}

// ===========================================================================
// Action Tests (Action.hpp)
// ===========================================================================

TEST(TestLeafNodes, SugarAction)
{
    int counter = 0;
    auto node = bt::Node::create<bt::SugarAction>([&counter]() {
        counter++;
        return bt::Status::SUCCESS;
    });

    EXPECT_TRUE(node->isValid());
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 2);
}

TEST(TestLeafNodes, SugarActionWithBlackboard)
{
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value", 42);

    auto node = bt::Node::create<bt::SugarAction>(
        [bb]() {
            auto val = bb->get<int>("value");
            if (val && *val == 42)
                return bt::Status::SUCCESS;
            return bt::Status::FAILURE;
        },
        bb);

    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, InvalidSugarAction)
{
    auto node = bt::Node::create<bt::SugarAction>(nullptr);
    EXPECT_FALSE(node->isValid());
}

// ===========================================================================
// Condition Tests (Condition.hpp)
// ===========================================================================

TEST(TestLeafNodes, Condition)
{
    bool flag = false;
    auto node = bt::Node::create<bt::Condition>([&flag]() { return flag; });

    EXPECT_TRUE(node->isValid());
    EXPECT_EQ(node->tick(), bt::Status::FAILURE);

    flag = true;
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, ConditionWithBlackboard)
{
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("enabled", true);

    auto node = bt::Node::create<bt::Condition>(
        [bb]() {
            auto enabled = bb->get<bool>("enabled");
            return enabled.value_or(false);
        },
        bb);

    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);

    bb->set("enabled", false);
    EXPECT_EQ(node->tick(), bt::Status::FAILURE);
}
