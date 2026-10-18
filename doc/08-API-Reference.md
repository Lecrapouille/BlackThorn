# 📖 8. API reference

The classes of the `bt` namespace, grouped by task. One include gives all of them:

```cpp
#include "BlackThorn/BlackThorn.hpp"
```

The editor (`oakular::Editor`) is documented in [src/Oakular/README.md](../src/Oakular/README.md).

---

## 🧱 1. Build a tree from YAML

### `bt::NodeFactory`

Maps the names of the YAML file to C++ code ([chapter 5](05-Your-Own-Nodes.md)).

| Method | Does |
|--------|------|
| `registerAction(name, std::function<Status()>)` | an `Action` from a function |
| `registerAction(name, function, blackboard)` | the same, with a blackboard for ports |
| `registerCondition(name, std::function<bool()>)` | a `Condition` from a function |
| `registerCondition(name, function, blackboard)` | the same, with a blackboard |
| `registerNode<T>(name)` | creates `T()` |
| `registerNode<T>(name, blackboard)` | creates `T(blackboard)` |
| `registerNode(name, std::function<std::unique_ptr<Node>()>)` | your own creator |
| `createNode(name)`, `hasNode(name)` | used by the builder |

### `bt::Builder`

All methods are static and return a `bt::Return<T>`.

| Method | Does |
|--------|------|
| `fromFile(factory, path, blackboard = nullptr, options = {})` | reads a YAML file and builds the tree |
| `fromText(factory, yaml, blackboard = nullptr, options = {})` | the same from a string |

When `blackboard` is null, the builder creates one. Pass yours to share it with your code.

### `bt::Return<T>`

The result of the builder: a value or an error message.

```cpp
auto result = bt::Builder::fromFile(factory, "tree.yaml", blackboard);
if (!result)                                  // or result.isError()
{
    std::cerr << result.getError() << std::endl;
    return 1;
}
bt::Tree::Ptr tree = result.moveValue();      // takes the tree out
```

| Method | Returns |
|--------|---------|
| `operator bool`, `isSuccess()`, `isError()` | the state |
| `getError()` | the message |
| `getValue()` | a reference to the value |
| `moveValue()` | the value, moved out |

---

## 🌲 2. Run a tree

### `bt::Tree`

| Method | Does |
|--------|------|
| `static Ptr create()` | a new empty tree (`Ptr` is `std::unique_ptr<Tree>`) |
| `Status tick()` | runs one tick from the root; `FAILURE` when there is no valid root |
| `Status status() const` | the status of the last tick |
| `void reset()` | puts every node back to `INVALID` (calls the reset functions of the actions) |
| `void halt()` | stops the running nodes (calls their `onHalt()`, then the reset functions of the actions) |
| `bool isValid() const` | the tree has a root and every node has its children |
| `void clock(std::function<time_point()>)` | the clock of the timed nodes; empty function = wall clock |
| `time_point now() const` | the current time of this clock |
| `setBlackboard(ptr)`, `blackboard()` | the root blackboard |
| `setVisualizerClient(ptr)`, `visualizerClient()` | sends the state to Oakular after each tick |
| `T& createRoot<T>(args...)` | creates the root node in C++ |
| `Node& getRoot()`, `nodes()`, `nodeCount()` | access to the nodes |
| `accept(visitor)` | walks the tree ([§5](#-5-walk-and-export-a-tree)) |

### `bt::Status`

```cpp
enum class bt::Status { INVALID = 0, RUNNING = 1, SUCCESS = 2, FAILURE = 3 };

std::string const& bt::to_string(bt::Status);   // "RUNNING", ...
```

---

## 🔨 3. Write nodes

### `bt::Node` (base of every node)

| Member | For |
|--------|-----|
| `std::string name` | the name shown in Oakular and in errors |
| `Status status() const` | the last status |
| `char const* typeName() const` | `"Sequence"`, `"Action"`, ... |
| `size_t index() const` | the place of the node in its tree |
| `blackboard()`, `setBlackboard(ptr)` | the blackboard of its scope |
| `getInput<T>(port)` *(protected)* | reads a port: `std::optional<T>` |
| `setOutput(port, value)` *(protected)* | writes a port |
| `virtual void onHalt()` *(protected)* | called when a running node is halted (before the reset function of a `CallbackLeaf`) |
| `virtual void onTearDown(Status)` *(protected)* | called when the node ends with `SUCCESS` or `FAILURE` |

### Leaves you derive from or create

| Class | Constructor |
|-------|-------------|
| `bt::CallbackLeaf` (YAML `Action`) | `(Function)`, `(Function, Blackboard::Ptr)`, `(Function, ResetFunction)` with `Function = std::function<Status()>`, `ResetFunction = std::function<void()>` |
| `bt::Condition` | `(std::function<bool()>)`, `(function, Blackboard::Ptr)` |

### Composites and decorators

| Base | Method | Does |
|------|--------|------|
| `bt::Composite` | `T& addChild<T>(args...)` | creates a child and adds it at the end |
| `bt::Decorator` | `T& createChild<T>(args...)` | creates the only child |

The constructors of every built-in node are in [chapter 4 §5](04-Nodes.md#-5-the-same-nodes-in-c).

---

## 🧠 4. Blackboard

### `bt::Blackboard`

| Method | Does |
|--------|------|
| `Blackboard(Ptr parent = nullptr)` | a blackboard, child of `parent` when given |
| `set(key, value)`, `set<T>(key, value)` | writes; the type is the type of `value`, or `T` |
| `std::optional<T> get<T>(key)` | reads; numbers convert without loss; empty if missing or not readable as `T` |
| `T getOrDefault<T>(key, default)` | reads, or `default` |
| `has(key)`, `remove(key)`, `keys()` | presence, removal, list |
| `Ptr createChild()` | a child scope (what subtrees use) |
| `std::string dump(title)` | a text listing for debugging |

Types: `bt::BlackboardValue` (a `std::variant` of `int`, `double`, `float`, `bool`, `std::string`, `size_t`, `std::vector<double>`, `std::vector<BlackboardValue>`, `bt::BlackboardMap` and `std::any`); `bt::BlackboardMap` is `std::unordered_map<std::string, BlackboardValue>`. Details: [chapter 6](06-Blackboard.md).

---

## 📤 5. Walk and export a tree

### `bt::Exporter`

| Static method | Returns |
|---------------|---------|
| `toYAML(tree)` | the YAML of the tree and of its blackboard, with `_id` |
| `toYAMLFile(tree, path)` | writes it to a file; `false` on error |
| `toYAMLStructure(tree)` | the tree only, without the blackboard |
| `blackboardToYAML(blackboard)` | the blackboard only |
| `toMermaid(tree)` | a [Mermaid](https://mermaid.js.org/) flowchart |

### Visitors

`bt::ConstBehaviorTreeVisitor` (read only) and `bt::BehaviorTreeVisitor` (read and write) have one `visitX(...)` method per node type (`visitSequence`, `visitAction`, ...). `tree.accept(visitor)` calls them. The exporter is written this way.

---

## 📡 6. Live view

### `bt::VisualizerClient`

Present when built with `BLACKTHORN_WITH_NETWORK=1` (the default); then `BLACKTHORN_HAS_NETWORK` is defined.

| Method | Does |
|--------|------|
| `VisualizerClient()` | a client, not connected |
| `bool connect(host = "localhost", port = 8888)` | connects to Oakular |
| `bool isConnected() const`, `void disconnect()` | connection state |

Give it to a tree with `tree->setVisualizerClient(client)`: the tree sends its structure, then its changes after every tick. The protocol is in [chapter 7 §4](07-Oakular.md#-4-the-protocol).

Next: [09-Examples.md](09-Examples.md).
