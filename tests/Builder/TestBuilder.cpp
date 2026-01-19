/**
 * @file TestBuilder.cpp
 * @brief Unit tests for Builder and tree construction from YAML.
 *
 * Corresponds to src/BlackThorn/Builder/Builder.hpp
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"
#include "BlackThorn/Builder/Builder.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path resolveExampleFile(std::string const& filename)
{
    namespace fs = std::filesystem;
    fs::path probe = fs::current_path();

    for (int i = 0; i < 7; ++i)
    {
        fs::path candidate = probe / "doc" / "examples" / filename;
        if (fs::exists(candidate))
        {
            return candidate;
        }
        if (!probe.has_parent_path())
        {
            break;
        }
        probe = probe.parent_path();
    }

    return {};
}

// ===========================================================================
// Custom Test Action Nodes
// ===========================================================================

class TestAction: public bt::Action
{
public:

    TestAction() : m_executed(false) {}

    bt::Status onRunning() override
    {
        m_executed = true;
        return bt::Status::SUCCESS;
    }

    bool wasExecuted() const
    {
        return m_executed;
    }

private:

    bool m_executed;
};

class TestCondition: public bt::Action
{
public:

    TestCondition() = default;

    bt::Status onRunning() override
    {
        return bt::Status::SUCCESS;
    }
};

} // anonymous namespace

// ===========================================================================
// Simple Tree Parsing Tests
// ===========================================================================

TEST(TestBuilder, ParseSimpleSuccessNode)
{
    std::string yaml = R"(
BehaviorTree:
  Success:
    name: TestSuccess
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    ASSERT_TRUE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseSimpleFailureNode)
{
    std::string yaml = R"(
BehaviorTree:
  Failure:
    name: TestFailure
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestBuilder, ParseSimpleSequence)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: MainSequence
    children:
      - Success:
          name: Step1
      - Success:
          name: Step2
      - Success:
          name: Step3
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_TRUE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseSimpleSelector)
{
    std::string yaml = R"(
BehaviorTree:
  Selector:
    name: MainSelector
    children:
      - Failure:
          name: FirstOption
      - Success:
          name: SecondOption
      - Failure:
          name: ThirdOption
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Nested Structure Tests
// ===========================================================================

TEST(TestBuilder, ParseNestedSequenceSelector)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: Root
    children:
      - Selector:
          name: ChooseAction
          children:
            - Failure:
                name: FirstChoice
            - Success:
                name: SecondChoice
      - Success:
          name: FinalStep
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseComplexNestedStructure)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: Root
    children:
      - Selector:
          name: Level1
          children:
            - Sequence:
                name: Level2A
                children:
                  - Failure:
                      name: Level3A
                  - Success:
                      name: Level3B
            - Success:
                name: Level2B
      - Success:
          name: FinalNode
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Decorator Tests
// ===========================================================================

TEST(TestBuilder, ParseInverter)
{
    std::string yaml = R"(
BehaviorTree:
  Inverter:
    name: NotSuccess
    child:
      - Success:
          name: ChildNode
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestBuilder, ParseRepeat)
{
    std::string yaml = R"(
BehaviorTree:
  Repeat:
    name: RepeatNode
    times: 5
    child:
      - Success:
          name: RepeatingChild
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_TRUE(tree->isValid());
}

TEST(TestBuilder, ParseForceSuccess)
{
    std::string yaml = R"(
BehaviorTree:
  ForceSuccess:
    name: AlwaysSucceed
    child:
      - Failure:
          name: FailingChild
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseForceFailure)
{
    std::string yaml = R"(
BehaviorTree:
  ForceFailure:
    name: AlwaysFail
    child:
      - Success:
          name: SuccessChild
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestBuilder, ParseRepeatUntilSuccess)
{
    std::string yaml = R"(
BehaviorTree:
  UntilSuccess:
    name: UntilSuccess
    attempts: 0
    child:
      - Success:
          name: EventualSuccess
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseUntilSuccessWithAttempts)
{
    std::string yaml = R"(
BehaviorTree:
  UntilSuccess:
    name: UntilSuccessLimited
    attempts: 3
    child:
      - Success:
          name: EventualSuccess
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseRepeatUntilFailure)
{
    std::string yaml = R"(
BehaviorTree:
  UntilFailure:
    name: UntilFailure
    attempts: 0
    child:
      - Failure:
          name: EventualFailure
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseUntilFailureWithAttempts)
{
    std::string yaml = R"(
BehaviorTree:
  UntilFailure:
    name: UntilFailureLimited
    attempts: 2
    child:
      - Failure:
          name: EventualFailure
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Parallel Tests
// ===========================================================================

TEST(TestBuilder, ParseParallelWithThresholds)
{
    std::string yaml = R"(
BehaviorTree:
  Parallel:
    name: ParallelNode
    success_threshold: 2
    failure_threshold: 2
    children:
      - Success:
          name: Child1
      - Success:
          name: Child2
      - Failure:
          name: Child3
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseParallelWithPolicies)
{
    std::string yaml = R"(
BehaviorTree:
  Parallel:
    name: ParallelAll
    success_on_all: true
    fail_on_all: false
    children:
      - Success:
          name: Child1
      - Success:
          name: Child2
      - Success:
          name: Child3
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Custom Action Tests
// ===========================================================================

TEST(TestBuilder, ParseCustomAction)
{
    std::string yaml = R"(
BehaviorTree:
  Action:
    name: CustomAction
)";

    bt::NodeFactory factory;
    factory.registerNode<TestAction>("CustomAction");

    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_TRUE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseCustomCondition)
{
    std::string yaml = R"(
BehaviorTree:
  Condition:
    name: CustomCondition
)";

    bt::NodeFactory factory;
    factory.registerNode<TestCondition>("CustomCondition");

    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseSequenceWithCustomActions)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: CustomSequence
    children:
      - Action:
          name: Action1
      - Action:
          name: Action2
      - Action:
          name: Action3
)";

    bt::NodeFactory factory;
    factory.registerNode<TestAction>("Action1");
    factory.registerNode<TestAction>("Action2");
    factory.registerNode<TestAction>("Action3");

    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, ParseSubTreeReference)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: Root
    children:
      - SubTree:
          name: AttackWrapper
          reference: Attack
      - Success:
          name: FinalStep
SubTrees:
  Attack:
    Sequence:
      name: Attack
      children:
        - Success:
            name: Acquire
        - Success:
            name: Execute
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Blackboard Integration Tests
// ===========================================================================

TEST(TestBuilder, ParseWithGlobalBlackboard)
{
    std::string yaml = R"(
Blackboard:
  counter: 42
  name: "TestName"
  enabled: true
  value: 3.14

BehaviorTree:
  Success:
    name: TestNode
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(bb->get<int>("counter"), 42);
    EXPECT_EQ(bb->get<std::string>("name"), "TestName");
    EXPECT_EQ(bb->get<bool>("enabled"), true);
    EXPECT_DOUBLE_EQ(bb->get<double>("value").value(), 3.14);
}

TEST(TestBuilder, ParseWithBlackboardVariables)
{
    std::string yaml = R"(
Blackboard:
  initial_value: 100
  multiplier: 2

BehaviorTree:
  Success:
    name: TestNode
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());

    EXPECT_TRUE(bb->has("initial_value"));
    EXPECT_TRUE(bb->has("multiplier"));
}

TEST(TestBuilder, ParseWithLocalParameters)
{
    std::string yaml = R"(
BehaviorTree:
  Action:
    name: TestAction
    parameters:
      local_param: 123
      local_string: "test"
)";

    bt::NodeFactory factory;
    factory.registerNode<TestAction>("TestAction");
    auto bb = std::make_shared<bt::Blackboard>();

    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(bb->get<int>("local_param"), 123);
    EXPECT_EQ(bb->get<std::string>("local_string"), "test");
}

TEST(TestBuilder, ParseNestedBlackboardStructures)
{
    std::string yaml = R"(
Blackboard:
  gains:
    kp: 10
    kd: 0.25
  waypoints:
    - 0.0
    - 1.5
    - 3.0

BehaviorTree:
  Success:
    name: TestNode
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());

    using VariantMap = std::unordered_map<std::string, std::any>;
    auto gains = bb->get<VariantMap>("gains");
    ASSERT_TRUE(gains.has_value());
    EXPECT_EQ(std::any_cast<int>(gains->at("kp")), 10);
    EXPECT_DOUBLE_EQ(std::any_cast<double>(gains->at("kd")), 0.25);

    auto waypoints = bb->get<std::vector<double>>("waypoints");
    ASSERT_TRUE(waypoints.has_value());
    ASSERT_EQ(waypoints->size(), 3U);
    EXPECT_DOUBLE_EQ((*waypoints)[1], 1.5);
}

TEST(TestBuilder, ParseBlackboardReferences)
{
    std::string yaml = R"(
Blackboard:
  base: 5
  copy: ${base}

BehaviorTree:
  Success:
    name: Root
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());
    auto copy = bb->get<int>("copy");
    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(copy.value(), 5);
}

// ===========================================================================
// Example File Integration Tests
// ===========================================================================

TEST(TestBuilderExamples, GameStateSubTreeFromFile)
{
    bt::NodeFactory factory;
    factory.registerNode<TestAction>("LoadGameState");
    factory.registerNode<TestAction>("ChoosePrimaryEnemy");

    auto yaml_path = resolveExampleFile("GameState/GameState.yaml");
    ASSERT_FALSE(yaml_path.empty()) << "Unable to locate example file";

    auto bb = std::make_shared<bt::Blackboard>();
    auto result =
        bt::Builder::fromFile(factory, yaml_path.string(), std::move(bb));

    ASSERT_TRUE(result.isSuccess()) << result.getError();
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilderExamples, PatrolAndEngageSubTreesFromFile)
{
    bt::NodeFactory factory;
    factory.registerNode<TestAction>("LoadRoute");
    factory.registerNode<TestAction>("FollowWaypoints");
    factory.registerNode<TestAction>("AttemptNonLethal");
    factory.registerNode<TestAction>("NeutralizeThreat");
    factory.registerNode<TestAction>("ExtractTeam");

    auto yaml_path = resolveExampleFile("Patrol/Patrol.yaml");
    ASSERT_FALSE(yaml_path.empty()) << "Unable to locate example file";

    auto result = bt::Builder::fromFile(factory, yaml_path.string());

    ASSERT_TRUE(result.isSuccess()) << result.getError();
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

// ===========================================================================
// Error Handling Tests
// ===========================================================================

TEST(TestBuilder, ErrorMissingBehaviorTree)
{
    std::string yaml = R"(
SomeOtherRoot:
  Success:
    name: Test
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("Missing 'BehaviorTree'"),
              std::string::npos);
}

TEST(TestBuilder, ErrorUnknownNodeType)
{
    std::string yaml = R"(
BehaviorTree:
  UnknownType:
    name: Test
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("Unknown node type"), std::string::npos);
}

TEST(TestBuilder, ErrorEmptySequence)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: EmptySeq
    children: []
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
}

TEST(TestBuilder, ErrorSequenceMissingChildren)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: NoChildren
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("missing 'children'"), std::string::npos);
}

TEST(TestBuilder, ErrorDecoratorWithMultipleChildren)
{
    std::string yaml = R"(
BehaviorTree:
  Inverter:
    name: TooManyChildren
    child:
      - Success:
          name: Child1
      - Success:
          name: Child2
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("exactly one child"), std::string::npos);
}

TEST(TestBuilder, ErrorDecoratorMissingChild)
{
    std::string yaml = R"(
BehaviorTree:
  Inverter:
    name: NoChild
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
}

TEST(TestBuilder, ErrorParallelMixedPoliciesAndThresholds)
{
    std::string yaml = R"(
BehaviorTree:
  Parallel:
    name: MixedConfig
    success_on_all: true
    success_threshold: 2
    children:
      - Success:
          name: Child1
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("both policies and thresholds"),
              std::string::npos);
}

TEST(TestBuilder, ErrorParallelMissingConfiguration)
{
    std::string yaml = R"(
BehaviorTree:
  Parallel:
    name: NoConfig
    children:
      - Success:
          name: Child1
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("policies or thresholds"),
              std::string::npos);
}

TEST(TestBuilder, ErrorInvalidYAML)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence: [
    - Success
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("YAML"), std::string::npos);
}

TEST(TestBuilder, ErrorActionNotInFactory)
{
    std::string yaml = R"(
BehaviorTree:
  Action:
    name: NonExistentAction
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("Failed to create"), std::string::npos);
}

TEST(TestBuilder, ErrorActionMissingName)
{
    std::string yaml = R"(
BehaviorTree:
  Action:
    parameters:
      value: 42
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    EXPECT_FALSE(result.isSuccess());
    EXPECT_NE(result.getError().find("missing 'name'"), std::string::npos);
}

// ===========================================================================
// File I/O Tests
// ===========================================================================

TEST(TestBuilder, ParseFromFile)
{
    // Create a temporary YAML file
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: FileSequence
    children:
      - Success:
          name: Step1
      - Success:
          name: Step2
)";

    std::string filename = "/tmp/test_tree.yaml";
    std::ofstream file(filename);
    file << yaml;
    file.close();

    bt::NodeFactory factory;
    auto result = bt::Builder::fromFile(factory, filename);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_TRUE(tree->isValid());
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);

    // Clean up
    std::remove(filename.c_str());
}

TEST(TestBuilder, ErrorFileNotFound)
{
    bt::NodeFactory factory;
    auto result = bt::Builder::fromFile(factory, "/nonexistent/path/tree.yaml");

    EXPECT_FALSE(result.isSuccess());
}

// ===========================================================================
// Complex Integration Tests
// ===========================================================================

TEST(TestBuilder, ComplexTreeWithAllNodeTypes)
{
    std::string yaml = R"(
Blackboard:
  max_retries: 3
  timeout: 5.0

BehaviorTree:
  Sequence:
    name: ComplexRoot
    children:
      - Selector:
          name: TryActions
          children:
            - UntilSuccess:
                name: RetryAction
                attempts: 3
                child:
                  - Action:
                      name: TestAction
            - ForceSuccess:
                name: FallbackSuccess
                child:
                  - Failure:
                      name: DefaultFail
      - Parallel:
          name: ParallelTasks
          success_threshold: 2
          failure_threshold: 1
          children:
            - Success:
                name: Task1
            - Success:
                name: Task2
            - Success:
                name: Task3
      - Inverter:
          name: CheckNotFailed
          child:
            - Failure:
                name: FailureCheck
)";

    bt::NodeFactory factory;
    factory.registerNode<TestAction>("TestAction");
    auto bb = std::make_shared<bt::Blackboard>();

    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_TRUE(tree->isValid());

    auto status = tree->tick();
    EXPECT_EQ(status, bt::Status::SUCCESS);
}

TEST(TestBuilder, TreeWithMultipleLevelsOfNesting)
{
    std::string yaml = R"(
BehaviorTree:
  Sequence:
    name: Level0
    children:
      - Selector:
          name: Level1A
          children:
            - Sequence:
                name: Level2A
                children:
                  - Selector:
                      name: Level3A
                      children:
                        - Failure:
                            name: Deep1
                        - Success:
                            name: Deep2
                  - Success:
                      name: Level3B
            - Success:
                name: Level2B
      - Success:
          name: Level1B
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestBuilder, MixedDecoratorsAndComposites)
{
    std::string yaml = R"(
BehaviorTree:
  Inverter:
    name: InvertRoot
    child:
      - Sequence:
          name: InnerSequence
          children:
            - ForceSuccess:
                name: ForceNode
                child:
                  - Selector:
                      name: DeepSelector
                      children:
                        - Failure:
                            name: FirstTry
                        - Failure:
                            name: SecondTry
            - Success:
                name: FinalSuccess
)";

    bt::NodeFactory factory;
    auto result = bt::Builder::fromText(factory, yaml);

    ASSERT_TRUE(result.isSuccess());
    auto tree = result.moveValue();
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE); // Inverted success
}

// ===========================================================================
// Blackboard Variable Resolution Tests
// ===========================================================================

TEST(TestBuilder, BlackboardArrays)
{
    std::string yaml = R"(
Blackboard:
  position: [1.0, 2.0, 3.0]
  values: [10, 20, 30, 40]

BehaviorTree:
  Success:
    name: TestNode
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());

    auto position = bb->get<std::vector<double>>("position");
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position->size(), 3);
    EXPECT_DOUBLE_EQ((*position)[0], 1.0);
    EXPECT_DOUBLE_EQ((*position)[1], 2.0);
    EXPECT_DOUBLE_EQ((*position)[2], 3.0);
}

TEST(TestBuilder, BlackboardMixedTypes)
{
    std::string yaml = R"(
Blackboard:
  count: 42
  ratio: 0.5
  message: "Hello"
  active: true
  coordinates: [1.0, 2.0]

BehaviorTree:
  Success:
    name: TestNode
)";

    bt::NodeFactory factory;
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromText(factory, yaml, bb);

    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(bb->get<int>("count"), 42);
    EXPECT_DOUBLE_EQ(bb->get<double>("ratio").value(), 0.5);
    EXPECT_EQ(bb->get<std::string>("message"), "Hello");
    EXPECT_EQ(bb->get<bool>("active"), true);

    auto coords = bb->get<std::vector<double>>("coordinates");
    ASSERT_TRUE(coords.has_value());
    EXPECT_EQ(coords->size(), 2);
}