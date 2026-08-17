# 🌳 BlackThorn - C++ behavior tree Library

![blackthorn](doc/logos/blackthorn.png)

> ⚠️ The API is still evolving. Expect breaking changes while the library matures.

**BlackThorn** is a modern C++17 behavior tree library designed for robotics, simulation, and games, featuring its integrated editor and real-time visualizer, **Oakular**.

A behavior tree (BT) is a mathematical model — specifically, a directed acyclic graph — for expressing decision logic in plan execution, serving as an alternative to state machines. Behavior trees describe the switching between a finite set of tasks in a modular fashion. While a BT can be represented as a state machine, such representations can become unwieldy. The key strength of behavior trees lies in their ability to compose highly complex tasks from simple ones, without requiring knowledge of how those simple tasks are implemented.

This project builds upon [BrainTree](https://github.com/arvidsson/BrainTree) by adding missing features such as blackboards, file-based tree descriptions, and an editor. It also serves as a modernization of the [BehaviorTree.CPP](https://www.behaviortree.dev/) which has a complex code base. Key distinctions are BlackThorn uses unified YAML files for both tree structure and blackboard data (instead of BehaviorTree.CPP's two-part XML approach: one for the tree, one for the editor), provides subtree support (absent in BrainTree) and scoped blackboards (missing some features compared to BehaviorTree.CPP), and includes a fully open-source editor and real-time visualizer (proprietary in BehaviorTree.CPP).

BlackThorn is a wordplay on "BT" (behavior tree) and serves as the name of the behavior tree library itself (the "tree"—prunellier or épine noire in French). Oakular is a wordplay combining "Oak" and "Ocular."

## 💻 Quick Start

- Prerequisites Ubuntu:

```bash
sudo apt-get install libsfml-dev
```

- Prerequisites Fedora:

```bash
sudo dnf install SFML-devel
```

- Download and compilation:

```bash
git clone https://github.com/Lecrapouille/BlackThorn --recurse

cd BlackThorn
make download-external-libs
make -j8          # builds library + editor + examples

./build/Oakular
./build/Example-GameState   # doc/examples/GameState
./build/Example-Patrol      # doc/examples/Patrol
./build/Example-Embedded    # doc/examples/Embedded
```

See [Getting Started Guide](doc/getting-started.md) for installation and basic usage.

## 🧩 Embedding BlackThorn in your own project

Both layers are embeddable. The behavior tree engine (`bt::`) has no graphical
dependency at all, and the editor (`oakular::`) is a pure Dear ImGui component:
it never creates a window, an OpenGL context or an ImGui context, and it never
calls GLFW. A host that already owns its window and its ImGui context writes:

```cpp
#include "Oakular/Oakular.hpp"

oakular::Editor m_editor;               // no width, no height, no window

// Expose your own domain nodes in the creation palette
m_editor.registerNodeType("MoveToTarget", "Robot");

// Once per frame, outside of the ImGui draw calls
void MyApp::onUpdate(float dt) { m_editor.update(dt); }

// Inside your menu bar, between BeginMenuBar and EndMenuBar
void MyApp::onDrawMenuBar() { m_editor.drawMenuBar(); }

// Inside your frame, between ImGui::NewFrame() and ImGui::Render()
void MyApp::onDrawMainPanel() { m_editor.draw("Behavior Tree Editor"); }
```

Actions the editor cannot perform on its own are signals your host connects to:
`onFileDialogRequested` (open your own file browser, then call `loadFromYaml` or
`saveToYaml`) and `onQuitRequested`. See
[doc/examples/Embedded](doc/examples/Embedded/Embedded.cpp) for a complete host,
and [src/Oakular/README.md](src/Oakular/README.md) for the full editor API.

### Two ways to consume the libraries

Define `BLACKTHORN_DIR` then include [BlackThorn.mk](BlackThorn.mk), which
exposes the sources, includes and flags of both layers:

```make
BLACKTHORN_DIR := $(P)/external/BlackThorn
IMGUI_DIR := $(P)/external/imgui      # reuse the ImGui clone of your project
include $(BLACKTHORN_DIR)/BlackThorn.mk
```

**Compile the sources** (simplest, and what `doc/examples/Embedded` does):

```make
INCLUDES += $(BLACKTHORN_INCLUDES) $(OAKULAR_INCLUDES)
VPATH += $(BLACKTHORN_VPATH) $(OAKULAR_VPATH)
SRC_FILES += $(BLACKTHORN_SOURCES) $(OAKULAR_SOURCES)
DEFINES += $(BLACKTHORN_DEFINES) $(OAKULAR_DEFINES)
PKG_LIBS += $(BLACKTHORN_PKG_LIBS) $(OAKULAR_PKG_LIBS)
USER_CXXFLAGS += $(OAKULAR_CXXFLAGS)
```

**Or link the pre-built archives**, after `make -C $(BLACKTHORN_DIR)`:

```make
INCLUDES += $(BLACKTHORN_INCLUDES) $(OAKULAR_INCLUDES)
DEFINES += $(BLACKTHORN_DEFINES) $(OAKULAR_DEFINES)
NOT_PKG_LIBS += $(OAKULAR_ARCHIVES) $(BLACKTHORN_ARCHIVES)
PKG_LIBS += $(BLACKTHORN_PKG_LIBS) $(OAKULAR_PKG_LIBS)
```

Drop the `OAKULAR_*` variables to embed the engine alone, without the editor.

### What the host must provide

- **Dear ImGui**, compiled exactly once in the final binary: `imgui.cpp`,
  `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`,
  `misc/cpp/imgui_stdlib.cpp`, plus the backends of your choice. Oakular
  consumes the headers only, so point `IMGUI_DIR` at your own clone to guarantee
  a single copy.
- **A file browser**, if you want load and save: Oakular does not impose
  `ImGuiFileDialog`, it emits `onFileDialogRequested` instead.
- **A window and an ImGui context**, created before the first `draw()`.

### Optional features

Both switches are off by default only if you say so; they default to enabled and
are the only reason `sfml-network` is needed:

| Switch | Default | Effect when set to `0` |
|---|---|---|
| `BLACKTHORN_WITH_NETWORK` | `1` | Drops `bt::VisualizerClient`: a tree can no longer stream its runtime state |
| `OAKULAR_WITH_SERVER` | `1` | Drops `oakular::Server`: the editor keeps its Creation mode but its Visualizer mode reports itself unavailable |

```bash
make BLACKTHORN_WITH_NETWORK=0 OAKULAR_WITH_SERVER=0 -j8   # no SFML at all
```

A host that wants the visualizer on its own terms keeps `OAKULAR_WITH_SERVER=1`
and injects its server with `editor.attachServer(...)`: the editor never creates
one by itself.

## Internal Documentation

- 📖 [Getting Started](doc/getting-started.md) - Installation and quick start
- 🌲 [Behavior Tree Primer](doc/bt-primer.md) - Core concepts, status, execution cycle, workflow
- 📚 [API Reference](doc/api-reference.md) - Complete API documentation
- 📝 [YAML Format](doc/yaml-format.md) - YAML file structure and syntax
- 🎯 [Node Types Guide](doc/nodes-guide.md) - All available node types
- 🧠 [Blackboard Guide](doc/blackboard-guide.md) - Blackboard usage and best practices
- 🖥️ [Visualizer Architecture & Guide](doc/visualizer-architecture.md) - Oakular visualizer architecture and usage
- 💡 [Examples Guide](doc/examples/examples-guide.md) - Complete examples walkthrough

## Additional Resources

- [Introduction to behavior trees](https://roboticseabass.com/2021/05/08/introduction-to-behavior-trees/) – roboticseabass.com
- [Behavior trees in Detail](https://lisyarus.github.io/blog/posts/behavior-trees.html) – lisyarus.github.io
- [BehaviorTree.CPP Documentation](https://www.behaviortree.dev/) – related library with similar concepts

## License

MIT License - see LICENSE file for details.
