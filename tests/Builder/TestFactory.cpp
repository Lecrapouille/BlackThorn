/**
 * @file TestFactory.cpp
 * @brief Unit tests for bt::NodeFactory.
 *
 * Corresponds to src/BlackThorn/Builder/Factory.hpp
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// bt::NodeFactory Tests
// ===========================================================================

TEST(TestNodeFactory, RegisterAndCreateNode)
{
    bt::NodeFactory factory;

    factory.registerNode<bt::Success>("MySuccess");

    auto node = factory.createNode("MySuccess");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestNodeFactory, CreateNonExistentNode)
{
    bt::NodeFactory factory;

    auto node = factory.createNode("NonExistent");
    EXPECT_EQ(node, nullptr);
}

TEST(TestNodeFactory, HasNode)
{
    bt::NodeFactory factory;
    factory.registerNode<bt::Success>("TestNode");

    EXPECT_TRUE(factory.hasNode("TestNode"));
    EXPECT_FALSE(factory.hasNode("OtherNode"));
}

TEST(TestNodeFactory, RegisterAction)
{
    bt::NodeFactory factory;
    int counter = 0;

    factory.registerAction("CountAction", [&counter]() {
        counter++;
        return bt::Status::SUCCESS;
    });

    auto node = factory.createNode("CountAction");
    ASSERT_NE(node, nullptr);

    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
}

TEST(TestNodeFactory, RegisterActionWithBlackboard)
{
    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value", 100);

    factory.registerAction(
        "BBAction",
        [bb]() {
            auto val = bb->get<int>("value");
            return val && *val == 100 ? bt::Status::SUCCESS
                                      : bt::Status::FAILURE;
        },
        bb);

    auto node = factory.createNode("BBAction");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestNodeFactory, RegisterCondition)
{
    bt::NodeFactory factory;
    bool flag = false;

    factory.registerCondition("TestCondition", [&flag]() { return flag; });

    auto node = factory.createNode("TestCondition");
    ASSERT_NE(node, nullptr);

    EXPECT_EQ(node->tick(), bt::Status::FAILURE);

    flag = true;
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestNodeFactory, RegisterConditionWithBlackboard)
{
    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("enabled", true);

    factory.registerCondition(
        "BBCondition",
        [bb]() { return bb->get<bool>("enabled").value_or(false); },
        bb);

    auto node = factory.createNode("BBCondition");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestNodeFactory, RegisterNodeWithBlackboard)
{
    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("test_value", 42);

    factory.registerAction(
        "BBAction",
        [bb]() {
            return bb->get<int>("test_value").value_or(0) == 42
                       ? bt::Status::SUCCESS
                       : bt::Status::FAILURE;
        },
        bb);

    auto node = factory.createNode("BBAction");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->tick(), bt::Status::SUCCESS);
}

TEST(TestNodeFactory, MultipleRegistrations)
{
    bt::NodeFactory factory;

    factory.registerNode<bt::Success>("SuccessNode");
    factory.registerNode<bt::Failure>("FailureNode");
    factory.registerAction("ActionNode", []() { return bt::Status::SUCCESS; });

    EXPECT_TRUE(factory.hasNode("SuccessNode"));
    EXPECT_TRUE(factory.hasNode("FailureNode"));
    EXPECT_TRUE(factory.hasNode("ActionNode"));

    auto success = factory.createNode("SuccessNode");
    auto failure = factory.createNode("FailureNode");
    auto action = factory.createNode("ActionNode");

    ASSERT_NE(success, nullptr);
    ASSERT_NE(failure, nullptr);
    ASSERT_NE(action, nullptr);

    EXPECT_EQ(success->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(failure->tick(), bt::Status::FAILURE);
    EXPECT_EQ(action->tick(), bt::Status::SUCCESS);
}
