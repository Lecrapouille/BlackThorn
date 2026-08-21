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

### Documents

The editor edits a document, and there is none until the host, or the user
through the `File` menu, asks for one. Without a document there is nothing to add
a node to: the palette, the auto-layout and both save entries stay disabled.

| Method | Purpose |
|---|---|
| `newDocument()` | Start an empty, unnamed document. This is what enables edition |
| `hasDocument()` | Whether a document is open, created or loaded |
| `isEditable()` | Whether the tree may be edited: a document, in creation mode |
| `save()` | Write the tree back to its file, or ask the host for a save dialog when it has none yet |

### Tree edition

`reset()`, `setMode()`, `mode()`, `addNode()`, `addNodeAndLink()`, `deleteNode()`,
`createLink()`, `deleteLink()`, `loadFromYaml()`, `loadFromYamlString()`,
`saveToYaml()`, `autoLayoutNodes()`, `setAutoLayoutEnabled()`,
`reorderChildrenByPosition()`, `toggleSubTreeExpansion()`, `isModified()`,
`filepath()`, `documentTitle()`.

An arc always goes from the output pin of a parent down to the input pin of a
child. `createLink()` refuses the opposite direction, from a node up to one of
its own ancestors, which would close a cycle and leave a graph no longer
serializable as a tree.

The root of a tab follows its structure: the first node created becomes it, a
node gaining a parent hands it over, and deleting it promotes its first child.
There is nothing to designate by hand.

### SubTrees and selection

`createSubTreeDefinition()`, `convertToSubTree()`, `inlineSubTree()`,
`selectedNodes()`, `selectNode()`, `toggleNodeSelection()`, `clearSelection()`,
`deleteSelection()`, `blackboard()`, `activeBlackboard()`.

`blackboard()` is the root scope, the one written to the `Blackboard` section.
`activeBlackboard()` is the scope of the open tab, a child of the root one for a
subtree.

### Extending the node palette

The palette lists the built-in node types `bt::Builder` knows how to
instantiate. A host declares its own domain nodes so that they can be placed
graphically, mirroring what it registers in its `bt::NodeFactory`:

```cpp
editor.registerNodeType("MoveToTarget", "Robot");
editor.registerNodeType("BatteryAboveThreshold", "Robot", /* can_have_ports */ true);
```

The category groups the entries in the palette, and decides how many children a
node takes: none for `Leaf`, one for `Decorator`, any number otherwise. Only the
node types that accept one more child offer a pin to drag a link from, and
`createLink()` refuses the ones that would overflow. A decorator is written under
the `child` key `bt::Builder` expects, everything else under `children`.

`can_have_ports` decides whether the node edition popup offers blackboard input
and output ports.

A port is a name and a binding: `${key}` to read or write that blackboard entry,
or a literal value. Both are written to the `parameters` block of the node, which
is what `bt::Builder` resolves.

### Attributes

Some node types are configured by keys the editor does not interpret: the `key`
and `value` of a `SetBlackboard`, the `times` of a `Repeat`, the `_id` the
visualizer correlates runtime state with. They are read from the file, listed in
the `Attributes` table of the node edition popup, and written back as they are,
so that opening a tree and saving it does not strip what makes it run.

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
| `Ctrl+N` | New behavior tree |
| `Ctrl+O` | Ask the host for a load dialog |
| `Ctrl+S` | Save, asking the host for a dialog when the tree has no file yet |
| `Ctrl+Shift+S` | Ask the host for a save dialog |
| `Ctrl+L` | Lay the tree out now |
| `Ctrl+A` | Select every node of the open tab |
| `Ctrl+Q` | Ask the host to quit |
| Right click | Open the node palette, or the menu of the node under the cursor |
| `Ctrl`/`Shift` + click | Add a node to the selection, or remove it |
| Left drag on empty canvas | Rubber band selection |
| Middle drag | Pan the canvas |
| `Delete` | Delete the selection |
| Double click | Edit a node, or open the definition of a SubTree |

The mouse only acts on the canvas: clicking in the blackboard panel or in a
dialog of the host leaves the graph alone.

## SubTrees

A SubTree node is a call site; what it runs is a definition edited in its own
tab. Picking `SubTree` in the palette creates both, so the definition is
reachable right away: double click the node, or use `Go to Definition`.

Two inverse operations factor behavior in and out:

| Operation | Effect |
|---|---|
| `Extract to SubTree` | Moves a node and its descendants into a new definition, and leaves a SubTree node calling it in their place |
| `Inline SubTree` | Replaces a SubTree node by the nodes of its definition, and drops the definition. Refused while another node still calls it |

### Blackboard scopes and port remapping

Each definition owns a blackboard scope nested in the one of the main tree,
mirroring the child blackboard `bt::Builder` creates at runtime: a key the
subtree does not define is read from the tree calling it.

The wiring lives on the SubTree node, as a table of ports: the left column is
the key seen inside the subtree, the right one the expression evaluated in the
calling tree. It is written to the `parameters` block the engine reads:

```yaml
- SubTree:
    name: MoveRobot
    reference: MoveRobot
    parameters:
      target: ${move_goal}    # read from the parent scope
      result: ${move_result}  # written back after the tick
    outputs:
      - result
```

`parameters` is the only port block `bt::Builder` understands. The `outputs`
list carries no meaning for the engine: the editor uses it to put the ports back
on the right side when the file is reopened, the direction being inferred at
build time from the blackboard.

The blackboard panel follows the open tab. A subtree scope is shown read-only,
since the file has no place to store it: what it holds is derived from the
remapping of the node calling it, plus the inherited keys.

## Layout

`Auto Layout` is a toggle, on by default: the tree is laid out again after every
structural change, so a node just created lands where the structure puts it.
Turn it off to place the nodes by hand, and use `Layout Now` on demand.

The order of the children follows their placement, left to right in a
top-to-bottom layout and top to bottom in a left-to-right one, the way Groot
behaves. Dragging a node past its sibling therefore changes the execution order,
and the file records what the canvas shows.

## Saving

The file written back is the one `bt::Builder` reads: `parameters` for the ports,
`child` or `children` depending on the node type, and the attributes the editor
does not interpret kept as they are. Definitions are written in alphabetical
order so that saving twice gives the same file.

Two conversions still happen through the blackboard and lose information, listed
in `TODO.txt`: a round double is written as an integer, and a `${var}` value of
the `Blackboard` section is written as the value it was resolved to.

## Future improvements

- [ ] Export to PNG or SVG
- [ ] Undo/Redo
- [ ] Copy and paste of subtrees
- [ ] Template library
- [ ] Step by step debugging in Visualizer mode

## License

MIT License - Copyright (c) 2025 Quentin Quadrat
