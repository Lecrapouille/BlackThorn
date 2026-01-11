/**
 * @file TestBlackboard.cpp
 * @brief Unit tests for Blackboard components: Blackboard, Ports, Resolver.
 *
 * Corresponds to src/BlackThorn/Blackboard/
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// Test Structures
// ===========================================================================

struct Position
{
    float x, y, z;

    bool operator==(const Position& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    void print() const
    {
        std::cout << "Position(" << x << ", " << y << ", " << z << ")";
    }
};

struct Enemy
{
    std::string name;
    int health;
    Position position;

    bool operator==(const Enemy& other) const
    {
        return name == other.name && health == other.health &&
               position == other.position;
    }

    void print() const
    {
        std::cout << "Enemy{" << name << ", HP:" << health << ", pos:";
        position.print();
        std::cout << "}";
    }
};

struct GameState
{
    std::vector<Enemy> enemies;
    int score;
    float time;

    void print() const
    {
        std::cout << "GameState{score:" << score << ", time:" << time
                  << ", enemies:" << enemies.size() << "}";
    }
};

// ===========================================================================
// Test Node Classes
// ===========================================================================

namespace {

// ****************************************************************************
//! \brief Test leaf node that reads a message from the blackboard.
//! \details Used for testing blackboard integration with node ports.
// ****************************************************************************
class PrintMessage: public bt::Leaf
{
public:

    explicit PrintMessage(bt::Blackboard::Ptr bb = nullptr) : bt::Leaf(bb) {}

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<std::string>("message");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (!m_blackboard)
            return bt::Status::FAILURE;
        if (auto msg = getInput<std::string>("message", *m_blackboard); msg)
        {
            m_last_message = *msg;
            return bt::Status::SUCCESS;
        }
        return bt::Status::FAILURE;
    }

    void setConfig(std::unordered_map<std::string, std::string> const& config)
    {
        configure(config);
    }

    std::string m_last_message;

    void accept(bt::ConstBehaviorTreeVisitor&) const override {}
    void accept(bt::BehaviorTreeVisitor&) override {}
};

// ****************************************************************************
//! \brief Test leaf node that performs addition using blackboard ports.
//! \details Used for testing input/output port integration with blackboard.
// ****************************************************************************
class Calculate: public bt::Leaf
{
public:

    explicit Calculate(bt::Blackboard::Ptr bb = nullptr) : bt::Leaf(bb) {}

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<int>("a");
        ports.addInput<int>("b");
        ports.addOutput<int>("result");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (!m_blackboard)
            return bt::Status::FAILURE;
        auto a = getInput<int>("a", *m_blackboard);
        auto b = getInput<int>("b", *m_blackboard);

        if (a && b)
        {
            int result = *a + *b;
            setOutput("result", result, *m_blackboard);
            return bt::Status::SUCCESS;
        }

        return bt::Status::FAILURE;
    }

    void setConfig(std::unordered_map<std::string, std::string> const& config)
    {
        configure(config);
    }

    void accept(bt::ConstBehaviorTreeVisitor&) const override {}
    void accept(bt::BehaviorTreeVisitor&) override {}
};

// ****************************************************************************
//! \brief Test leaf node that processes Position structs from blackboard.
//! \details Used for testing custom struct types with blackboard ports.
// ****************************************************************************
class MoveToPosition: public bt::Leaf
{
public:

    explicit MoveToPosition(bt::Blackboard::Ptr bb = nullptr) : bt::Leaf(bb) {}

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<Position>("target");
        ports.addOutput<Position>("current_pos");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (!m_blackboard)
            return bt::Status::FAILURE;
        auto target = getInput<Position>("target", *m_blackboard);

        if (target)
        {
            // Simulate movement
            Position current{
                target->x * 0.5f, target->y * 0.5f, target->z * 0.5f};
            setOutput("current_pos", current, *m_blackboard);
            return bt::Status::SUCCESS;
        }

        return bt::Status::FAILURE;
    }

    void setConfig(std::unordered_map<std::string, std::string> const& config)
    {
        configure(config);
    }

    void accept(bt::ConstBehaviorTreeVisitor&) const override {}
    void accept(bt::BehaviorTreeVisitor&) override {}
};

// ****************************************************************************
//! \brief Test leaf node that processes Enemy structs from blackboard.
//! \details Used for testing complex custom struct types with blackboard.
// ****************************************************************************
class ProcessEnemy: public bt::Leaf
{
public:

    explicit ProcessEnemy(bt::Blackboard::Ptr bb = nullptr) : bt::Leaf(bb) {}

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<Enemy>("enemy");
        ports.addOutput<bool>("is_dead");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (!m_blackboard)
            return bt::Status::FAILURE;
        auto enemy = getInput<Enemy>("enemy", *m_blackboard);

        if (enemy)
        {
            bool dead = enemy->health <= 0;
            setOutput("is_dead", dead, *m_blackboard);
            return bt::Status::SUCCESS;
        }

        return bt::Status::FAILURE;
    }

    void setConfig(std::unordered_map<std::string, std::string> const& config)
    {
        configure(config);
    }

    void accept(bt::ConstBehaviorTreeVisitor&) const override {}
    void accept(bt::BehaviorTreeVisitor&) override {}
};

} // anonymous namespace

// ===========================================================================
// Basic Blackboard Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test basic set and get operations on blackboard.
//! \details GIVEN a blackboard, WHEN setting different types of values,
//!          THEN EXPECT the values can be retrieved correctly.
// ------------------------------------------------------------------------
TEST(TestBlackboard, BasicSetGet)
{
    // GIVEN: A blackboard
    auto bb = std::make_shared<bt::Blackboard>();

    // WHEN: Setting different types of values
    bb->set("int_value", 42);
    bb->set("double_value", 3.14);
    bb->set("string_value", std::string("hello"));
    bb->set("bool_value", true);

    // THEN: EXPECT the values can be retrieved correctly
    EXPECT_EQ(bb->get<int>("int_value"), 42);
    EXPECT_DOUBLE_EQ(bb->get<double>("double_value").value(), 3.14);
    EXPECT_EQ(bb->get<std::string>("string_value"), "hello");
    EXPECT_EQ(bb->get<bool>("bool_value"), true);
}

// ------------------------------------------------------------------------
//! \brief Test blackboard type safety.
//! \details GIVEN a blackboard with a value of a specific type, WHEN
//!          retrieving with the correct type, THEN EXPECT success. WHEN
//!          retrieving with wrong types, THEN EXPECT nullopt.
// ------------------------------------------------------------------------
TEST(TestBlackboard, TypeSafety)
{
    // GIVEN: A blackboard with an integer value
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value", 42);

    // WHEN: Retrieving with the correct type
    // THEN: EXPECT success
    EXPECT_TRUE(bb->get<int>("value").has_value());
    EXPECT_EQ(bb->get<int>("value").value(), 42);

    // WHEN: Retrieving with wrong types
    // THEN: EXPECT nullopt
    EXPECT_FALSE(bb->get<double>("value").has_value());
    EXPECT_FALSE(bb->get<std::string>("value").has_value());
}

// ------------------------------------------------------------------------
//! \brief Test blackboard key existence check.
//! \details GIVEN a blackboard with a key, WHEN checking for existing
//!          and non-existing keys, THEN EXPECT correct boolean results.
// ------------------------------------------------------------------------
TEST(TestBlackboard, HasKey)
{
    // GIVEN: A blackboard with a key
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("existing", 123);

    // WHEN: Checking for existing and non-existing keys
    // THEN: EXPECT correct boolean results
    EXPECT_TRUE(bb->has("existing"));
    EXPECT_FALSE(bb->has("non_existing"));
}

// ------------------------------------------------------------------------
//! \brief Test blackboard key removal.
//! \details GIVEN a blackboard with a key, WHEN removing the key, THEN
//!          EXPECT the key no longer exists.
// ------------------------------------------------------------------------
TEST(TestBlackboard, Remove)
{
    // GIVEN: A blackboard with a key
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("temp", 456);

    EXPECT_TRUE(bb->has("temp"));

    // WHEN: Removing the key
    bb->remove("temp");

    // THEN: EXPECT the key no longer exists
    EXPECT_FALSE(bb->has("temp"));
}

// ------------------------------------------------------------------------
//! \brief Test blackboard with custom structs.
//! \details GIVEN a blackboard, WHEN setting and retrieving a custom
//!          struct, THEN EXPECT the struct values are preserved correctly.
// ------------------------------------------------------------------------
TEST(TestBlackboard, CustomStructs)
{
    // GIVEN: A blackboard
    auto bb = std::make_shared<bt::Blackboard>();

    // WHEN: Setting and retrieving a custom struct
    Position pos{1.0f, 2.0f, 3.0f};
    bb->set("position", pos);

    auto retrieved = bb->get<Position>("position");

    // THEN: EXPECT the struct values are preserved correctly
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->x, 1.0f);
    EXPECT_EQ(retrieved->y, 2.0f);
    EXPECT_EQ(retrieved->z, 3.0f);
}

// ------------------------------------------------------------------------
//! \brief Test blackboard with complex nested structs.
//! \details GIVEN a blackboard, WHEN setting and retrieving a complex
//!          struct with nested data, THEN EXPECT all struct members are
//!          preserved correctly.
// ------------------------------------------------------------------------
TEST(TestBlackboard, ComplexStructs)
{
    // GIVEN: A blackboard
    auto bb = std::make_shared<bt::Blackboard>();

    // WHEN: Setting and retrieving a complex struct
    Enemy enemy{"Goblin", 50, {10.0f, 0.0f, 5.0f}};
    bb->set("enemy", enemy);

    auto retrieved = bb->get<Enemy>("enemy");

    // THEN: EXPECT all struct members are preserved correctly
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "Goblin");
    EXPECT_EQ(retrieved->health, 50);
    EXPECT_EQ(retrieved->position.x, 10.0f);
    EXPECT_EQ(retrieved->position.y, 0.0f);
    EXPECT_EQ(retrieved->position.z, 5.0f);
}

// ===========================================================================
// Hierarchical Blackboard Tests
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test parent-child blackboard hierarchy.
//! \details GIVEN a parent and child blackboard, WHEN accessing values,
//!          THEN EXPECT child can access parent values, child can override
//!          parent values, and parent cannot access child values.
// ------------------------------------------------------------------------
TEST(TestBlackboard, ParentChildHierarchy)
{
    // GIVEN: A parent and child blackboard
    auto parent = std::make_shared<bt::Blackboard>();
    parent->set("parent_value", 100);
    parent->set("shared_value", 200);

    auto child = std::make_shared<bt::Blackboard>(parent);
    child->set("child_value", 300);
    child->set("shared_value", 400); // Override parent value

    // WHEN: Accessing values
    // THEN: EXPECT child can access its own values
    EXPECT_EQ(child->get<int>("child_value"), 300);

    // THEN: EXPECT child can access parent values
    EXPECT_EQ(child->get<int>("parent_value"), 100);

    // THEN: EXPECT child's value overrides parent's
    EXPECT_EQ(child->get<int>("shared_value"), 400);

    // THEN: EXPECT parent cannot access child values
    EXPECT_FALSE(parent->get<int>("child_value").has_value());
    EXPECT_EQ(parent->get<int>("shared_value"), 200);
}

// ------------------------------------------------------------------------
//! \brief Test blackboard child creation.
//! \details GIVEN a parent blackboard, WHEN creating a child blackboard,
//!          THEN EXPECT child can access parent data, child can have its
//!          own data, and parent cannot access child data.
// ------------------------------------------------------------------------
TEST(TestBlackboard, CreateChild)
{
    // GIVEN: A parent blackboard
    auto parent = std::make_shared<bt::Blackboard>();
    parent->set("parent_data", std::string("from parent"));

    // WHEN: Creating a child blackboard
    auto child = parent->createChild();
    child->set("child_data", std::string("from child"));

    // THEN: EXPECT child can access parent data and has its own data
    EXPECT_TRUE(child->has("parent_data"));
    EXPECT_TRUE(child->has("child_data"));
    EXPECT_FALSE(parent->has("child_data"));
}

// ===========================================================================
// Variable Resolution Tests (Resolver.hpp)
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test variable resolver string resolution.
//! \details GIVEN a blackboard with a variable and a template string,
//!          WHEN resolving the template, THEN EXPECT the variable is
//!          substituted correctly.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveString)
{
    // GIVEN: A blackboard with a variable and a template string
    bt::Blackboard bb;
    bb.set("name", std::string("World"));

    // WHEN: Resolving the template
    std::string result = bt::VariableResolver::resolve("Hello ${name}!", bb);

    // THEN: EXPECT the variable is substituted correctly
    EXPECT_EQ(result, "Hello World!");
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver with multiple variables.
//! \details GIVEN a blackboard with multiple variables and a template
//!          string, WHEN resolving the template, THEN EXPECT all variables
//!          are substituted correctly.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveMultipleVariables)
{
    // GIVEN: A blackboard with multiple variables
    bt::Blackboard bb;
    bb.set("first", std::string("John"));
    bb.set("last", std::string("Doe"));

    // WHEN: Resolving a template with multiple variables
    std::string result = bt::VariableResolver::resolve("${first} ${last}", bb);

    // THEN: EXPECT all variables are substituted correctly
    EXPECT_EQ(result, "John Doe");
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver value resolution for integers.
//! \details GIVEN a blackboard with an integer variable, WHEN resolving
//!          the value, THEN EXPECT the correct integer value is returned.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveValueInt)
{
    // GIVEN: A blackboard with an integer variable
    bt::Blackboard bb;
    bb.set("count", 42);

    // WHEN: Resolving the integer value
    auto result = bt::VariableResolver::resolveValue<int>("${count}", bb);

    // THEN: EXPECT the correct integer value is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver with literal integer values.
//! \details GIVEN a literal integer string, WHEN resolving the value,
//!          THEN EXPECT the literal integer is parsed correctly.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveValueLiteralInt)
{
    // GIVEN: A literal integer string
    bt::Blackboard bb;

    // WHEN: Resolving the literal integer value
    auto result = bt::VariableResolver::resolveValue<int>("123", bb);

    // THEN: EXPECT the literal integer is parsed correctly
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver value resolution for doubles.
//! \details GIVEN a blackboard with a double variable, WHEN resolving
//!          the value, THEN EXPECT the correct double value is returned.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveValueDouble)
{
    // GIVEN: A blackboard with a double variable
    bt::Blackboard bb;
    bb.set("pi", 3.14159);

    // WHEN: Resolving the double value
    auto result = bt::VariableResolver::resolveValue<double>("${pi}", bb);

    // THEN: EXPECT the correct double value is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14159);
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver value resolution for strings.
//! \details GIVEN a blackboard with a string variable, WHEN resolving
//!          the value, THEN EXPECT the correct string value is returned.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveValueString)
{
    // GIVEN: A blackboard with a string variable
    bt::Blackboard bb;
    bb.set("message", std::string("Hello"));

    // WHEN: Resolving the string value
    auto result =
        bt::VariableResolver::resolveValue<std::string>("${message}", bb);

    // THEN: EXPECT the correct string value is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Hello");
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver value resolution for booleans.
//! \details GIVEN literal boolean strings, WHEN resolving the values,
//!          THEN EXPECT the correct boolean values are parsed.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveValueBool)
{
    // GIVEN: Literal boolean strings
    bt::Blackboard bb;

    // WHEN: Resolving boolean values
    auto result_true = bt::VariableResolver::resolveValue<bool>("true", bb);
    auto result_false = bt::VariableResolver::resolveValue<bool>("false", bb);

    // THEN: EXPECT the correct boolean values are parsed
    ASSERT_TRUE(result_true.has_value());
    EXPECT_TRUE(*result_true);
    ASSERT_TRUE(result_false.has_value());
    EXPECT_FALSE(*result_false);
}

// ------------------------------------------------------------------------
//! \brief Test variable resolver with non-existent variables.
//! \details GIVEN a template with a non-existent variable, WHEN resolving,
//!          THEN EXPECT the variable remains unchanged in the result.
// ------------------------------------------------------------------------
TEST(TestVariableResolver, ResolveNonExistentVariable)
{
    // GIVEN: A template with a non-existent variable
    bt::Blackboard bb;

    // WHEN: Resolving the template
    std::string result = bt::VariableResolver::resolve("${missing}", bb);

    // THEN: EXPECT the variable remains unchanged
    EXPECT_EQ(result, "${missing}");
}

// ===========================================================================
// Port System Tests (Ports.hpp)
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test port list input and output registration.
//! \details GIVEN a port list, WHEN adding input and output ports,
//!          THEN EXPECT ports are correctly registered and can be
//!          identified by type.
// ------------------------------------------------------------------------
TEST(TestPortList, AddInputOutput)
{
    // GIVEN: A port list
    bt::PortList ports;

    // WHEN: Adding input and output ports
    ports.addInput<int>("input1");
    ports.addInput<std::string>("input2",
                                std::optional<std::string>("default"));
    ports.addOutput<double>("output1");

    // THEN: EXPECT ports are correctly registered and can be identified
    EXPECT_TRUE(ports.isInput("input1"));
    EXPECT_TRUE(ports.isInput("input2"));
    EXPECT_TRUE(ports.isOutput("output1"));
    EXPECT_FALSE(ports.isOutput("input1"));
    EXPECT_FALSE(ports.isInput("output1"));
}

// ===========================================================================
// Node Integration Tests with Blackboard
// ===========================================================================

// ------------------------------------------------------------------------
//! \brief Test node integration with blackboard for string messages.
//! \details GIVEN a node with a blackboard containing a message, WHEN
//!          configuring and executing the node, THEN EXPECT the message
//!          is read correctly from the blackboard.
// ------------------------------------------------------------------------
TEST(TestNodeWithBlackboard, PrintMessage)
{
    // GIVEN: A node with a blackboard containing a message
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("msg", std::string("Hello from Blackboard"));

    auto node = std::make_unique<PrintMessage>(bb);

    // WHEN: Configuring and executing the node
    std::unordered_map<std::string, std::string> config;
    config["message"] = "${msg}";
    node->setConfig(config);

    bt::Status status = node->tick();

    // THEN: EXPECT the message is read correctly from the blackboard
    EXPECT_EQ(status, bt::Status::SUCCESS);
    EXPECT_EQ(node->m_last_message, "Hello from Blackboard");
}

// ------------------------------------------------------------------------
//! \brief Test node integration with blackboard for calculations.
//! \details GIVEN a node with input values in the blackboard, WHEN
//!          configuring and executing the node, THEN EXPECT the calculation
//!          result is written to the blackboard correctly.
// ------------------------------------------------------------------------
TEST(TestNodeWithBlackboard, CalculateWithInputs)
{
    // GIVEN: A node with input values in the blackboard
    auto bb = std::make_shared<bt::Blackboard>();
    bb->set("value_a", 10);
    bb->set("value_b", 32);

    auto node = std::make_unique<Calculate>(bb);

    // WHEN: Configuring and executing the node
    std::unordered_map<std::string, std::string> config;
    config["a"] = "${value_a}";
    config["b"] = "${value_b}";
    config["result"] = "${sum}";
    node->setConfig(config);

    bt::Status status = node->tick();
    EXPECT_EQ(status, bt::Status::SUCCESS);

    // THEN: EXPECT the calculation result is written to the blackboard
    auto result = bb->get<int>("sum");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

// ------------------------------------------------------------------------
//! \brief Test node integration with blackboard for struct types.
//! \details GIVEN a node with a struct in the blackboard, WHEN configuring
//!          and executing the node, THEN EXPECT the struct is processed
//!          and the result is written correctly.
// ------------------------------------------------------------------------
TEST(TestNodeWithBlackboard, MoveToPositionStruct)
{
    // GIVEN: A node with a struct in the blackboard
    auto bb = std::make_shared<bt::Blackboard>();
    Position target{100.0f, 200.0f, 300.0f};
    bb->set("target_pos", target);

    auto node = std::make_unique<MoveToPosition>(bb);

    // WHEN: Configuring and executing the node
    std::unordered_map<std::string, std::string> config;
    config["target"] = "${target_pos}";
    config["current_pos"] = "${current}";
    node->setConfig(config);

    bt::Status status = node->tick();
    EXPECT_EQ(status, bt::Status::SUCCESS);

    // THEN: EXPECT the struct is processed and result is written correctly
    auto current = bb->get<Position>("current");
    ASSERT_TRUE(current.has_value());
    EXPECT_FLOAT_EQ(current->x, 50.0f);
    EXPECT_FLOAT_EQ(current->y, 100.0f);
    EXPECT_FLOAT_EQ(current->z, 150.0f);
}

// ------------------------------------------------------------------------
//! \brief Test node integration with blackboard for complex structs.
//! \details GIVEN a node with a complex struct in the blackboard, WHEN
//!          configuring and executing the node, THEN EXPECT the complex
//!          struct is processed and the result is written correctly.
// ------------------------------------------------------------------------
TEST(TestNodeWithBlackboard, ProcessEnemyComplex)
{
    // GIVEN: A node with a complex struct in the blackboard
    auto bb = std::make_shared<bt::Blackboard>();
    Enemy enemy{"Orc", 0, {5.0f, 0.0f, 10.0f}};
    bb->set("current_enemy", enemy);

    auto node = std::make_unique<ProcessEnemy>(bb);

    // WHEN: Configuring and executing the node
    std::unordered_map<std::string, std::string> config;
    config["enemy"] = "${current_enemy}";
    config["is_dead"] = "${dead}";
    node->setConfig(config);

    bt::Status status = node->tick();
    EXPECT_EQ(status, bt::Status::SUCCESS);

    // THEN: EXPECT the complex struct is processed and result is written
    auto is_dead = bb->get<bool>("dead");
    ASSERT_TRUE(is_dead.has_value());
    EXPECT_TRUE(*is_dead); // Health is 0, so enemy is dead
}

// ------------------------------------------------------------------------
//! \brief Test node integration with blackboard using literal values.
//! \details GIVEN a node configured with literal values instead of
//!          variables, WHEN executing the node, THEN EXPECT the literal
//!          values are parsed and the result is written correctly.
// ------------------------------------------------------------------------
TEST(TestNodeWithBlackboard, LiteralValues)
{
    // GIVEN: A node configured with literal values
    auto bb = std::make_shared<bt::Blackboard>();

    auto node = std::make_unique<Calculate>(bb);

    // WHEN: Configuring with literal values and executing the node
    std::unordered_map<std::string, std::string> config;
    config["a"] = "5";
    config["b"] = "7";
    config["result"] = "${output}";
    node->setConfig(config);

    bt::Status status = node->tick();
    EXPECT_EQ(status, bt::Status::SUCCESS);

    // THEN: EXPECT the literal values are parsed and result is written
    auto result = bb->get<int>("output");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 12);
}
