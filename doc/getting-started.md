# 🚀 Getting Started with BlackThorn

This guide will help you get started with BlackThorn, a modern C++ behavior tree library.

## 📦 Prerequisites

```bash
sudo apt-get install libsfml-dev
```

## 🔨 Download and Compilation

```bash
git clone https://github.com/Lecrapouille/Blackthorn --recurse
cd Blackthorn
make download-external-libs
make -j8          # builds library + examples
make test -j8     # optional unit tests
```

## 👁️ Running Oakular (Editor and Visualizer)

BlackThorn comes with **Oakular** - a standalone editor and visualizer application:

```bash
./build/Oakular --port 9090
```

Oakular can work in two modes:

- **Editor mode**: Create and edit behavior trees graphically
- **Visualizer mode**: Display runtime behavior trees from TCP clients

A blank window will appear waiting for a connection with a client application using this library. Launch it before a demo to see live node states.

The editor is also embeddable in your own application: `oakular::Editor` is a pure
Dear ImGui component that draws inside the frame you own. See the
[embedding section of the README](../README.md#-embedding-blackthorn-in-your-own-project)
and `doc/examples/Embedded/`.

## 🏃 Running Examples

```bash
make examples -j8
./build/Example-GameState   # doc/examples/GameState
./build/Example-Patrol      # doc/examples/Patrol
./build/Example-Embedded    # doc/examples/Embedded
```

Each example folder contains its own `Makefile`, YAML description, and C++ entry point. Use them as templates for your own projects.

## ⚡ Quick Example

### 📝 Using YAML (Recommended)

Create a `tree.yaml` file:

```yaml
BehaviorTree:
  Sequence:
    children:
      - Action:
          name: "OpenDoor"
      - Action:
          name: "Walk"
      - Action:
          name: "CloseDoor"
```

Then in your C++ code:

```cpp
#include "BlackThorn/BlackThorn.hpp"
#include "BlackThorn/Builder.hpp"

auto builder = bt::Builder::create();
auto result = builder->loadFromFile("tree.yaml");
if (result.hasError()) {
    std::cerr << "Error: " << result.getError() << std::endl;
    return;
}

auto tree = result.getTree();
tree->tick();  // Execute the tree
```

## 👉 Next Steps

- Read the [Behavior Tree Primer](bt-primer.md) for core concepts and fundamentals
- Read the [API Reference](api-reference.md) for detailed class documentation
- Learn about [YAML Format](yaml-format.md) for defining trees
- Explore the [Node Types Guide](nodes-guide.md)
- Check out the [Examples Guide](examples/examples-guide.md)
