# Oakular - Embeddable behavior tree editor

Oakular is the graphical layer of BlackThorn. It offers two modes:

- **Creation**: build and edit behavior trees graphically, load and save them as YAML.
- **Visualizer**: display, in real time, the execution of a tree running in another
  process, fed over TCP by a `bt::VisualizerClient`.

## The embedding contract

`oakular::Editor` is a pure Dear ImGui component. It guarantees three things, and
they are what makes it embeddable in any host:

1. It never creates a window, an OpenGL context or an ImGui context, and never
   calls GLFW. The host owns the frame.
2. It compiles no third-party source into its archive. It consumes the Dear ImGui
   headers only, at a path the host imposes through `IMGUI_DIR`, so that a single
   copy of ImGui ends up in the final binary.
3. It exposes no symbol in the global namespace: everything lives in `oakular`,
   and the engine in `bt`.

Anything the editor cannot do on its own — browsing the file system, closing the
window — it asks for through a signal.

## Minimal host

```cpp
#include "Oakular/Oakular.hpp"

oakular::Editor editor;                 // no width, no height, no window

// Once per frame, outside of the ImGui draw calls.
editor.update(delta_time);

// Inside the frame, between ImGui::NewFrame() and ImGui::Render().
editor.draw("Behavior Tree Editor");

// Inside your menu bar, between BeginMenuBar and EndMenuBar.
editor.drawMenuBar();
```

`doc/examples/Embedded/Embedded.cpp` is a complete host: its own GLFW window, its
own ImGui context, its own dockspace, and no dependency on the `Application`
layer of the standalone application.

## API

### Construction

| Method | Purpose |
|---|---|
| `Editor()` | Empty tree with its own blackboard |
| `Editor(bt::Blackboard::Ptr)` | Share a blackboard owned by the host, so the edited tree and the rest of the host see the same variables |
| `setBlackboard(ptr)` | Replace the edited blackboard later on |
| `blackboard()` | Retrieve it, to hand it over to a running `bt::Tree` |

### Life cycle, driven by the host

| Method | When to call it |
|---|---|
| `setup()` | Optional. The constructor already initializes the editor; use it to restart from scratch |
| `update(float dt)` | Once per frame, outside of the ImGui draw calls. Polls the visualizer server |
| `teardown()` | Optional. Called by the destructor |

### Drawing, inside an ImGui frame

| Method | Draws |
|---|---|
| `draw(title)` | Everything: keyboard shortcuts, tree window and blackboard panel |
| `drawEditorWindow(title)` | The tree window alone |
| `drawBlackboardPanel()` | The blackboard panel alone, so the host can dock it where it wants |
| `drawMenuBar()` | The File, Edit, View and Mode menus, to be called from the host menu bar |
| `drawBehaviorTree()` | The canvas alone, at the current ImGui cursor |

### Tree edition

`reset()`, `setMode()`, `mode()`, `addNode()`, `addNodeAndLink()`, `deleteNode()`,
`createLink()`, `deleteLink()`, `loadFromYaml()`, `loadFromYamlString()`,
`saveToYaml()`, `autoLayoutNodes()`, `toggleSubTreeExpansion()`, `isModified()`,
`filepath()`, `selectedNode()`.

### Extending the node palette

The palette lists the ten built-in node types. A host declares its own domain
nodes so that they can be placed graphically, mirroring what it registers in its
`bt::NodeFactory`:

```cpp
editor.registerNodeType("MoveToTarget", "Robot");
editor.registerNodeType("BatteryAboveThreshold", "Robot", /* can_have_ports */ true);
```

The category groups the entries in the palette. `can_have_ports` decides whether
the node edition popup offers blackboard input and output ports.

### Signals

| Signal | Meaning |
|---|---|
| `onFileDialogRequested(FileDialog)` | The user asked to load or save. Open your own file browser, then call `loadFromYaml` or `saveToYaml` |
| `onQuitRequested()` | The user asked to close. Check `isModified()` and decide |
| `onNodeModified(ID)` | A node was edited through the popup |
| `onLinkCreated(ID, ID)` | A link was created |
| `onLinkDeleted(ID)` | A link was deleted |

### Visualizer server

The editor never creates a server: the host injects one, which is what lets an
embedded editor stay away from TCP entirely.

```cpp
editor.attachServer(std::make_shared<oakular::Server>());
```

Available only when built with `OAKULAR_WITH_SERVER=1` (the default), which
defines `OAKULAR_HAS_SERVER`. Without a server the Visualizer mode reports itself
unavailable and the Creation mode is unaffected.

## Files

```
src/Oakular/
├── Oakular.hpp          # Umbrella header for hosts
├── Editor.hpp/cpp       # Tree model, YAML I/O, layout
├── EditorWidgets.cpp    # Dear ImGui widgets of the editor
├── TreeRenderer.hpp/cpp # Canvas: nodes, links, pan and zoom
├── Server.hpp/cpp       # Optional TCP server for the Visualizer mode
└── Oakular.deps.mk      # Compile flags shared with the hosts
```

## Dependencies

- **Dear ImGui**: headers only. The host compiles it, `misc/cpp/imgui_stdlib.cpp`
  included, and owns the context.
- **BlackThorn**: the behavior tree engine, `libblackthorn.a`.
- **SFML Network**: only for the optional visualizer server.

Notably absent: GLFW, GLEW and OpenGL are dependencies of the host, not of the
editor. `ImGuiFileDialog` is a choice of the host too.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+O` | Ask the host for a load dialog |
| `Ctrl+S` | Ask the host for a save dialog |
| `Ctrl+L` | Auto layout |
| `Ctrl+Q` | Ask the host to quit |
| `Space` or right click | Open the node palette |
| `Delete` | Delete the selected node |
| Double click | Edit a node, or open the definition of a SubTree |

## Future improvements

- [ ] Link validation (no cycles)
- [ ] Export to PNG or SVG
- [ ] Undo/Redo
- [ ] Copy and paste of subtrees
- [ ] Template library
- [ ] Step by step debugging in Visualizer mode

## License

MIT License - Copyright (c) 2025 Quentin Quadrat
