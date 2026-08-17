# 🌟 Examples Guide

BlackThorn comes with several complete examples demonstrating different features. You can use examples as templates. Each example folder contains:

- 🛠️ `Makefile` - Build configuration
- 🧾 `*.cpp` - C++ source code
- 🌲 `*.yaml` - Tree definition (where applicable)
- 🧩 `ExampleUtilities.hpp` - Helper utilities (where applicable)

✨ Copy an example folder and modify it to create your own behavior trees!

---

## 📦 Node Examples

📁 Location: `doc/examples/Nodes/`

Individual examples for each node type:
- `LeafExample.cpp` - 🏃 Action, ❓ Condition, ✅ Success, ❌ Failure
- `SequenceExample.cpp` - ➡️ Sequence composite
- `SelectorExample.cpp` - 🔀 Selector composite
- `ParallelExample.cpp` - 🤝 Parallel composites
- `DecoratorExample.cpp` - 🎭 Various decorators
- `SubTreeExample.yaml` - 🌳 SubTree usage

---

## 🕹️ GameState Example

📁 Location: `doc/examples/GameState/`

Demonstrates:
- 🧠 Complex blackboard structures with nested maps
- 🌳 Subtree composition
- 📦 Parameter passing between subtrees
- 👀 Using the visualizer

▶️ Run: `./build/Example-GameState`

---

## 👮 Patrol Example

📁 Location: `doc/examples/Patrol/`

Demonstrates:
- 📝 YAML-based tree construction
- 🔗 Connecting to Oakular visualizer
- 🔄 Running trees in a loop with automatic state updates

▶️ Run: `./build/Example-Patrol`

To use with visualizer:
1. 🎨 Launch Oakular in Visualizer mode (before the example)
2. ▶️ Run the example
3. 👁️ Watch the tree execute in real-time

---

## 🧩 Embedded Example

📁 Location: `doc/examples/Embedded/`

Embeds the Oakular editor into a host application, the way another project such
as a robot simulator would. Demonstrates:

- 🪟 A GLFW window and a Dear ImGui context created by the host, not by Oakular
- 🧱 Composing `oakular::Editor` as a plain member, without inheriting any window layer
- 🎨 Docking the editor into the dockspace of the host
- 🤖 Declaring domain nodes with `registerNodeType` so they appear in the palette
- 📡 Answering the `onFileDialogRequested` and `onQuitRequested` signals

Unlike the other examples, its `Makefile` consumes BlackThorn through
[BlackThorn.mk](../../BlackThorn.mk), exactly as an external project would.

▶️ Run: `./build/Example-Embedded`
