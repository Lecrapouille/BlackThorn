# 📝 API Reference

Complete API reference for BlackThorn behavior tree library.

---

## 🚦 Status

Enum representing the execution status of a node. Every node in a behavior tree returns one of these statuses when executed.

```cpp
enum class bt::Status {
    INVALID = 0,  //!< ❓ Invalid state (internal use, indicates node needs initialization)
    RUNNING = 1,  //!< 🔁 Node is currently executing and needs more time
    SUCCESS = 2,  //!< ✅ Node completed successfully
    FAILURE = 3   //!< ❌ Node failed or condition was not met
};

// Convert status enum to human-readable string
std::string const& to_string(Status status);
```

**Example:**

```cpp
auto status = tree->tick();
if (status == bt::Status::RUNNING) {
    // Node still executing, will be ticked again next frame
} else {
   // Handle success or failure
   std::cout << bt::to_string(status) << std::endl;
} else
```

---

## 🌳 Node

Base class for all nodes in the behavior tree. All behavior tree nodes inherit from this class.

### Public Methods

#### 🏭 Factory Method

```cpp
template<typename T, typename... Args>
static std::unique_ptr<T> create(Args&&... args)
```

Creates a new node of type `T` with the provided constructor arguments.

#### 🔎 Type of node

```cpp
std::string const& type() const
```

Will return for example "Parallel" or "Sequence" ...

#### 🛡️ Validation

```cpp
virtual bool isValid() const = 0
```

Pure virtual method that must be implemented by derived classes to validate node structure (e.g., composite has children, decorator has a child). To be called once after the tree has been correctly created (for example missing child nodes).

#### 🏃 Execution

```cpp
Status tick()
```

Execute the node. This method implements the template method pattern, calling `onSetUp()`, `onRunning()`, and `onTearDown()` as appropriate.

#### 🚦 Status Access

```cpp
Status status() const
```

Get the current execution status and node type identifier.

#### 🧹 State Management

```cpp
void reset()
void halt()
```

Reset the node to INVALID state (forces re-initialization on next tick) or halt a running node.

#### 👀 Visitor Pattern

```cpp
virtual void accept(ConstBehaviorTreeVisitor& visitor) const = 0
virtual void accept(BehaviorTreeVisitor& visitor) = 0
```

Accept visitors for tree traversal and modification.

### Public Methods (for configuration):

#### 🗃️ Blackboard Access

```cpp
Blackboard::Ptr blackboard() const
void setBlackboard(Blackboard::Ptr const& bb)
```

Get or set the blackboard for the node. The blackboard is automatically set by the Builder when loading from YAML.

#### 🔌 Port Remapping

```cpp
void setPortRemapping(std::unordered_map<std::string, std::string> const& remapping)
```

Configure the mapping between port names and blackboard keys. This is typically called by the Builder based on the `parameters:` section in YAML.

### Protected Methods (for derived classes):

#### 🔌 Port Access

```cpp
template<typename T>
std::optional<T> getInput(std::string const& port) const

template<typename T>
void setOutput(std::string const& port, T&& value)
```

Access blackboard values through ports. The port name is resolved to a blackboard key using the port remapping configuration.

#### 🕰️ Lifecycle Hooks

```cpp
virtual Status onSetUp()          // Called once before first execution
virtual Status onRunning() = 0    // Called every tick (must be implemented)
virtual void onTearDown(Status)   // Called when node completes (SUCCESS/FAILURE)
virtual void onHalt()             // Called when halt() is invoked on running node
```

---

## 🌲 Tree

Container for a behavior tree instance. The Tree class owns the root node and manages execution and state.

**Key Methods:**

- **Construction 🛠️:**

```cpp
static Ptr create()
```

Factory method to create a new tree instance.

- **Root Node Management ⚓:**

```cpp
template<class T, typename... Args>
T& createRoot(Args&&... args)

void setRoot(Node::Ptr root)
Node& getRoot() / Node const& getRoot() const
```

Create or set the root node of the tree. The root is the entry point for tree execution.

- **Blackboard Management 🗃️:**

```cpp
void setBlackboard(Blackboard::Ptr bb)
Blackboard::Ptr blackboard() const
```

Set or get the shared blackboard for the tree. All nodes in the tree can access this blackboard.

- **Execution 🏁:**

```cpp
Status tick()
```

Execute one tick of the tree. This calls `tick()` on the root node and propagates execution down the tree.

- **Validation ✅:**

```cpp
bool isValid() const
```

Validate that the tree has a root node and all nodes in the tree are valid.

- **State Management 🔄:**

```cpp
void reset()
void halt()
Status status() const
```

Reset or halt the entire tree (recursively resets/halts all nodes). Get the last execution status.

- **Visualization 👁️:**

```cpp
void setVisualizerClient(std::shared_ptr<VisualizerClient> visualizer)
std::shared_ptr<VisualizerClient> visualizerClient() const
```

Attach a visualizer client for real-time tree monitoring. The tree automatically sends state updates after each tick.

---

## Composite 🧱

Base class for nodes with multiple children. Composite nodes control execution flow by managing how and when their children are executed.

**Key Methods:**

- **Child Management 👶:**

```cpp
void addChild(Node::Ptr child)
template<class T, typename... Args>
T& addChild(Args&&... args)
```

Add a child node to the composite. The template version creates and adds a new node in one call, returning a reference for further configuration.

- **Child Access 🔗:**

```cpp
bool hasChildren() const
std::vector<Node::Ptr> const& getChildren() const
std::vector<Node::Ptr>& getChildren()
```

Check if the composite has children and access the children list.

- **Inherited from Node ⬆️:**

All methods from Node class, including `tick()`, `reset()`, `halt()`, etc.

**Common Derived Classes:**

- `Sequence` - Execute children in order until one fails ➡️
- `Selector` - Try children in order until one succeeds 🎯
- `Parallel` - Execute all children simultaneously 🔀

---

## Decorator 🎁

Base class for nodes with a single child. Decorators modify the behavior of their child node by transforming its status or adding additional logic.

**Key Methods:**

- **Child Management 👶:**

```cpp
void setChild(Node::Ptr child)
template<class T, typename... Args>
T& createChild(Args&&... args)
```

Set or create the single child node of the decorator.

- **Child Access 🔎:**

```cpp
bool hasChild() const
Node& getChild() / Node const& getChild() const
```

Check if the decorator has a child and access it.

- **Inherited from Node ⬆️:**

All methods from Node class.

**Common Derived Classes:**

- `Inverter` - Flips SUCCESS ↔ FAILURE 🔃
- `Retry` - Retries child until success or max attempts ♻️
- `Repeater` - Repeats child N times (can read count from blackboard) 🔁
- `Timeout` - Fails if child doesn't complete in time (can read duration from blackboard) ⏱️
- `ForceSuccess` / `ForceFailure` - Always return a fixed status 🎲

---

## Leaf 🍃

Base class for terminal nodes (no children). Leaf nodes perform actual work - they are the endpoints where behavior tree logic interacts with your application.

**Key Methods:**

- **Inherited from Node ⬆️:**

All methods from Node class, including `blackboard()`, `setBlackboard()`, `getInput()`, `setOutput()`. Leaf nodes must implement `onRunning()` to define their behavior.

**Common Derived Classes:**

- `Action` - Abstract base for custom actions (override `onRunning()`) 🔨
- `Condition` - Evaluates a boolean condition ❓
- `Success` / `Failure` - Always return a fixed status ✅❌
- `SugarAction` - Action from lambda function (for quick prototyping) 💡

---

## Utility Classes 🛠️

### Blackboard 🗃️

Shared key-value storage for node communication. The blackboard allows nodes to exchange data without direct references to each other, enabling modular and reusable behavior trees.

**Key Methods:**

- **Storage 💾:**

```cpp
template<typename T>
void set(std::string const& key, T&& value)
```

Store a value in the blackboard. The YAML types (`int`, `double`, `float`, `bool`, `std::string`, `size_t`, arrays and maps) land in a dedicated variant alternative; any other copyable C++ type is boxed in `std::any`.

- **Retrieval 🕵️:**

```cpp
template<typename T>
std::optional<T> get(std::string const& key) const

template<typename T>
T getOrDefault(std::string const& key, T default_value) const

bool has(std::string const& key) const
```

Retrieve values from the blackboard. `get()` returns `std::optional` (empty if key not found), `getOrDefault()` provides a default value.

- **Management 🧹:**

```cpp
void remove(std::string const& key)
std::shared_ptr<Blackboard> createChild()
```

Remove a key or create a child blackboard (used for subtree scope isolation).

**Usage Example:** 🧑‍💻

```cpp
auto blackboard = std::make_shared<bt::Blackboard>();
blackboard->set<int>("battery", 75);
blackboard->set<std::string>("target", "Enemy-A");

auto battery = blackboard->get<int>("battery");
if (battery && *battery > 50) {
    // Battery is sufficient
}
```

### Builder 🏗️

Create behavior trees from YAML files or strings. The Builder parses YAML descriptions and constructs the corresponding tree structure with nodes and blackboard values.

**Key Methods:**

- **Construction 🛠️:**

```cpp
static std::unique_ptr<Builder> create()
```

Create a new Builder instance.

- **Loading Trees 📂:**

```cpp
Result loadFromFile(std::string const& filepath)
Result loadFromString(std::string const& yaml_content)
```

Load a behavior tree from a YAML file or string. Returns a `Result` object containing the tree and any error messages.

- **Custom Nodes 🧑‍🔬:**

```cpp
void registerNodeFactory(NodeFactory::Ptr factory)
```

Register a NodeFactory for creating custom node types from YAML.

**Usage Example:** 🧑‍💻

```cpp
auto builder = bt::Builder::create();
auto result = builder->loadFromFile("my_tree.yaml");
if (result.hasError()) {
    std::cerr << "Error: " << result.getError() << std::endl;
    return;
}
auto tree = result.getTree();
auto blackboard = result.getBlackboard();
```

### VisualizerClient 👁️‍🗨️

TCP client for real-time tree visualization.

**Key Methods:**

- `VisualizerClient(std::string const& host, uint16_t port)`
- `bool connect()` - Connect to Oakular
- `bool isConnected() const`
- `void sendStateChanges(Tree const& tree)` - Send current state

### Exporter 📤

Utility class to export behavior trees to various formats (YAML, Mermaid).

**Key Methods:**

- **YAML Export 📄:**

```cpp
static std::string toYAML(Tree const& tree)
static bool toYAMLFile(Tree const& tree, std::string const& path)
static std::string toYAMLStructure(Tree const& tree)
```

Export a tree to YAML format. `toYAML()` includes Blackboard data, `toYAMLStructure()` exports only the tree structure. The YAML includes `_id` fields for each node.

- **Blackboard Export 🗃️:**

```cpp
static std::string blackboardToYAML(Blackboard const& blackboard)
```

Export just the blackboard contents to YAML format.

- **Mermaid Export 📊:**

```cpp
static std::string toMermaid(Tree const& tree)
```

Export the tree as a Mermaid flowchart diagram.

**Usage Example:** 🧑‍💻

```cpp
bt::Tree tree;
// ... build tree ...

// Export to YAML string
std::string yaml = bt::Exporter::toYAML(tree);

// Export to file
bt::Exporter::toYAMLFile(tree, "my_tree.yaml");

// Export to Mermaid diagram
std::string mermaid = bt::Exporter::toMermaid(tree);
```

### NodeFactory 🏭

Factory for creating nodes by name. Allows registration of custom node types that can be instantiated from YAML by name.

**Key Methods:**

- **Registration 🪪:**

```cpp
void registerNode(std::string const& name, NodeCreator creator)
template<typename T>
void registerNode(std::string const& name)
template<typename T>
void registerNode(std::string const& name, Blackboard::Ptr blackboard)
```

Register a node type with a name. The template versions automatically create appropriate factory functions.

- **Creation 🧬:**

```cpp
std::unique_ptr<Node> createNode(std::string const& name) const
bool hasNode(std::string const& name) const
```

Create a node instance by name or check if a name is registered.

- **Convenience Methods 🪄:**

```cpp
void registerAction(std::string const& name, SugarAction::Function&& func)
void registerAction(std::string const& name, SugarAction::Function&& func, Blackboard::Ptr blackboard)
void registerCondition(std::string const& name, Condition::Function&& func)
void registerCondition(std::string const& name, Condition::Function&& func, Blackboard::Ptr blackboard)
```

Register action and condition nodes from lambda functions.

**Usage Example:** 🧑‍💻

```cpp
auto factory = std::make_shared<bt::NodeFactory>();
factory->registerNode<MyCustomAction>("MyCustomAction");
factory->registerAction("QuickAction", []() {
    // Do something
    return bt::Status::SUCCESS;
});
```

## Visitor Pattern 🕵️‍♂️

The visitor pattern allows you to traverse and operate on behavior tree structures without modifying the node classes themselves.

### ConstBehaviorTreeVisitor ✋

Read-only visitor interface for tree traversal. Provides visit methods for all node types that receive const references.

**Usage:** 🧑‍💻

```cpp
class MyVisitor : public bt::ConstBehaviorTreeVisitor {
    void visitSequence(Sequence const& node) override { /* ... */ }
    void visitAction(Action const& node) override { /* ... */ }
    // ... implement all visit methods
};
```

### BehaviorTreeVisitor ✍️

Read-write visitor interface for tree modification. Similar to ConstBehaviorTreeVisitor but allows modification of nodes.

### DFSOrderVisitor 🧭

Concrete visitor that collects nodes in depth-first pre-order for visualization. This ensures consistent node ordering between the runtime tree and the visualizer.

**Usage:** 🧑‍💻

```cpp
DFSOrderVisitor visitor;
tree.accept(visitor);
for (auto* node : visitor.nodes) {
    // Process nodes in DFS order
}
```

## Helper Functions 🧰

### getDFSOrder 🗺️

```cpp
std::vector<Node const*> getDFSOrder(Tree const& tree);
```

Convenience function that returns nodes in depth-first pre-order. This is useful for consistent visualization and tree analysis.

**Usage:** 🧑‍💻

```cpp
auto nodes = bt::getDFSOrder(*tree);
for (auto* node : nodes) {
    std::cout << node->type() << std::endl;
}
```
