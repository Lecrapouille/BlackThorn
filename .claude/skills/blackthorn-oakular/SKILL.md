---
name: blackthorn-oakular
description: Change Oakular, the BlackThorn editor and live visualizer (src/Oakular: Editor, EditorWidgets, TreeRenderer, Server; applications/Oakular), its TCP protocol with bt::VisualizerClient, or how a host embeds it (BlackThorn.mk, signals, registerNodeType, attachServer). Use for editor UI, palette, layout, save/load, visualizer colors, embedding problems (duplicate ImGui, missing context).
---

# Oakular: editor, visualizer, embedding

## The embedding contract (never break it)

`oakular::Editor` (src/Oakular) is a pure Dear ImGui component:

1. never creates a window, an OpenGL context or an ImGui context, never calls
   GLFW (only `applications/Oakular` and `doc/examples/Embedded` do);
2. compiles no third-party source: ImGui headers only, from the target
   `imgui` of the host, so the final binary has one ImGui;
3. exports nothing in the global namespace (`oakular::`, `bt::`).

What it cannot do, it asks through a signal: `onFileDialogRequested`,
`onQuitRequested` (also `onNodeModified`, `onLinkCreated`, `onLinkDeleted`).
The visualizer server is injected: `editor.attachServer(...)`.
Host frame order: `update(dt)` outside ImGui draw calls; `draw(title)` (or
`drawEditorWindow` / `drawBlackboardPanel` / `drawBehaviorTree`) between
`NewFrame` and `Render`; `drawMenuBar()` inside the host menu bar.
RobotIK embeds it in its simulator by reading `BlackThorn.mk` with MyMakefile
3: keep the names of the targets `blackthorn` and `oakular`, and of the
options `BLACKTHORN_WITH_NETWORK`, `OAKULAR_WITH_SERVER`.

## Files

| File | Role |
|------|------|
| `Editor.hpp/.cpp` | document, tree model, YAML load/save, palette (`m_node_types` list), layout, modes |
| `EditorWidgets.cpp` | ImGui widgets: popups, blackboard panel, visualizer panel |
| `TreeRenderer.hpp/.cpp` | canvas: nodes, links, pan/zoom, colors per type and per status |
| `Server.hpp/.cpp` | SFML TCP server, port 8888 by default (`Server(port)`, app option `--port N`), only with `OAKULAR_WITH_SERVER=1` (`OAKULAR_HAS_SERVER`) |
| `applications/Oakular/` | standalone app: GLFW window, ImGuiFileDialog, wires the signals |

## Protocol (client `src/BlackThorn/Network/VisualizerClient.cpp`)

TCP text. Once: `YAML:<tree with _id on every node>\nEND_YAML`. After each
tick, changed nodes only: `S:id:status,id:status,...\n` with status
`0` INVALID, `1` RUNNING, `2` SUCCESS, `3` FAILURE. Nodes are matched by
`_id` (builder assigns missing ids; exporter always writes them). Change
both sides together and doc/07-Oakular.md §4.

## Rules

- Save writes exactly what `bt::Builder` reads: `parameters` for ports,
  `child` for decorators, `children` otherwise, unknown attributes kept as
  they are, `_id` kept. Test a load → save → load round trip.
- Child order follows canvas position (left to right): moving a node changes
  execution order.
- A new built-in node type needs a palette entry and a color
  (see `blackthorn-node-type`).
- User-facing behavior changes go to `src/Oakular/README.md` (editor API,
  shortcuts) and doc/07-Oakular.md (modes, protocol, embedding).

Try it: `./build/linux-gcc-release/bin/Oakular`; `./build/linux-gcc-release/bin/Example-Embedded`; Visualizer mode +
`./build/linux-gcc-release/bin/Example-StatusDemo`.
