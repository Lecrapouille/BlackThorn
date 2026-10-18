# 🌟 9. Examples

Every folder of `doc/examples/` is a small program that you can copy to start your own. Each one holds a `Makefile`, a C++ file and, most of the time, a YAML tree. `make` builds them all into `build/Example-<Name>`; run them from the root of the repository, because they open their YAML file with a path such as `doc/examples/Patrol/Patrol.yaml`.

| Example | Shows | Visualizer |
|---------|-------|------------|
| [Nodes](#-nodes) | one small program per node family | no |
| [Patrol](#-patrol) | subtrees, ports, structured data | yes |
| [GameState](#-gamestate) | a deep blackboard (maps in lists in maps) | yes |
| [StatusDemo](#-statusdemo) | every status color, with random results | yes |
| [SubTreeRemapping](#-subtreeremapping) | inputs and outputs of a subtree | no |
| [Embedded](#-embedded) | the Oakular editor inside a host window | — |

For the examples marked "yes", start `./build/Oakular`, choose **Mode → Visualizer**, then run the example ([chapter 7](07-Oakular.md)).

---

## 📦 Nodes

📁 `doc/examples/Nodes/` · ▶️ `./build/Example-Nodes <family>`, with `<family>` one of `leaf`, `sequence`, `selector`, `parallel`, `decorator`.

| File | Nodes |
|------|-------|
| `LeafExample.cpp` | 🔨 Action, ❓ Condition, ✅ Success, ❌ Failure |
| `SequenceExample.cpp` | ▶️ the sequences |
| `SelectorExample.cpp` | ➡️ the selectors |
| `ParallelExample.cpp` | ⏸️ the parallels |
| `DecoratorExample.cpp` | 🎭 the decorators |
| `SubTreeExample.yaml` | 🪵 a subtree, in YAML |

Read them next to [chapter 4](04-Nodes.md).

---

## 👮 Patrol

📁 `doc/examples/Patrol/` · ▶️ `./build/Example-Patrol`

A guard follows a route, meets an intruder, tries to talk, then acts, and leaves. Shows:

- 📝 a tree built from YAML, with two `SubTrees`;
- 🔌 actions that read their ports with `getInput<ValueMap>(...)`;
- 🗺️ waypoints and positions as maps of the blackboard;
- 👁️ the connection to the visualizer.

---

## 🎮 GameState

📁 `doc/examples/GameState/` · ▶️ `./build/Example-GameState`

A game state with a score, a time and a list of enemies, each with a position. Shows:

- 🧠 nested maps and lists in the `Blackboard` section;
- 🌳 subtrees that receive part of this structure as parameters;
- 👀 the visualizer.

---

## 🚥 StatusDemo

📁 `doc/examples/StatusDemo/` · ▶️ `./build/Example-StatusDemo`

Actions that succeed, fail or run at random. Made to see every color of Oakular change: green, yellow, red and gray.

---

## 🔁 SubTreeRemapping

📁 `doc/examples/SubTreeRemapping/` · ▶️ `./build/Example-SubTreeRemapping`

The main tree gives `move_goal` to a subtree as `target` (an input), and gets `result` back as `move_result` (an output), then prints it. The YAML file is the reference of [chapter 6 §4](06-Blackboard.md#-4-scopes-subtrees-have-their-own-blackboard).

---

## 🧩 Embedded

📁 `doc/examples/Embedded/` · ▶️ `./build/Example-Embedded`

The Oakular editor inside a host application, as a robot simulator would do it. Shows:

- 🪟 a GLFW window and a Dear ImGui context created by the host, not by Oakular;
- 🧱 `oakular::Editor` as a plain member, without any window class;
- 🎨 the editor docked in the dockspace of the host;
- 🤖 domain nodes added to the palette with `registerNodeType`;
- 📡 the `onFileDialogRequested` and `onQuitRequested` signals.

Unlike the others, its `Makefile` uses [BlackThorn.mk](../BlackThorn.mk), exactly as a project outside this repository would ([chapter 7 §5](07-Oakular.md#-5-embed-oakular-in-your-application)).

---

✨ To start your own program: copy the folder closest to your need, rename the `TARGET_NAME` of its `Makefile`, and edit the YAML file and the actions.
