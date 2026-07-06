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
    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::Success>();
    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, FailureNode)
{
    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::Failure>();
    EXPECT_EQ(node.tick(), bt::Status::FAILURE);
    EXPECT_EQ(node.tick(), bt::Status::FAILURE);
}

// ===========================================================================
// Callback tests (Callback.hpp)
// ===========================================================================

TEST(TestLeafNodes, CallbackLeaf)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::CallbackLeaf>([&counter]() {
        counter++;
        return bt::Status::SUCCESS;
    });

    EXPECT_TRUE(node.isValid());
    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 2);
}

TEST(TestLeafNodes, CallbackLeafWithBlackboard)
{
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value", 42);

    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::CallbackLeaf>(
        [bb]() {
            auto val = bb->get<int>("value");
            if (val && *val == 42)
                return bt::Status::SUCCESS;
            return bt::Status::FAILURE;
        },
        bb);

    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, InvalidCallback)
{
    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::CallbackLeaf>(nullptr);
    EXPECT_FALSE(node.isValid());
}

// ===========================================================================
// Condition Tests (Condition.hpp)
// ===========================================================================

TEST(TestLeafNodes, Condition)
{
    bool flag = false;
    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::Condition>([&flag]() { return flag; });

    EXPECT_TRUE(node.isValid());
    EXPECT_EQ(node.tick(), bt::Status::FAILURE);

    flag = true;
    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);
}

TEST(TestLeafNodes, ConditionWithBlackboard)
{
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("enabled", true);

    auto tree = bt::Tree::create();
    auto& node = tree->emplaceNode<bt::Condition>(
        [bb]() {
            auto enabled = bb->get<bool>("enabled");
            return enabled.value_or(false);
        },
        bb);

    EXPECT_EQ(node.tick(), bt::Status::SUCCESS);

    bb->set("enabled", false);
    EXPECT_EQ(node.tick(), bt::Status::FAILURE);
}
