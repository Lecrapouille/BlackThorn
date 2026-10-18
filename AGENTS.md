# AGENTS.md — context for coding agents

BlackThorn: C++17 behavior tree library (`bt::`, no graphical dependency)
plus Oakular, a Dear ImGui editor and live visualizer (`oakular::`,
embeddable, never creates a window). Trees are YAML files (tree, blackboard,
subtrees in one file). Linux, plain `make` (MyMakefile in `.makefile/`, no
CMake). MIT license.

Read only what the task needs. Load a skill instead of reading the docs:

| Task | Skill (`.claude/skills/`) | Long doc |
|------|---------------------------|----------|
| Write a YAML tree, register actions/conditions, ports, clock | `blackthorn-tree` | doc/03, doc/05, doc/06 |
| Add or change a built-in node type in the engine | `blackthorn-node-type` | doc/04-Nodes.md |
| Change the Oakular editor, visualizer, protocol, embedding | `blackthorn-oakular` | doc/07-Oakular.md, src/Oakular/README.md |
| Edit the documentation | `blackthorn-docs` | doc/README.md |
| Find a class or method | — | doc/08-API-Reference.md |

## Layout

```text
src/BlackThorn/   the engine (bt::)
  Nodes/          Node (Kind enum, NodeConfig), Tree (data-oriented runtime:
                  setUpNode/runNode/haltNode/resetNode in Tree.cpp), SubTree,
                  Composites/ Decorators/ Leaves/
  Builder/        Builder (YAML -> Tree, getNodeCreators), Factory (NodeFactory),
                  Exporter (Tree -> YAML/Mermaid), Yaml (rapidyaml wrapper)
  Blackboard/     Blackboard, BlackboardValue (variant), Ports, Resolver, Serializer
  Network/        VisualizerClient (only with BLACKTHORN_WITH_NETWORK=1)
  Visitors/       ConstBehaviorTreeVisitor, BehaviorTreeVisitor
src/Oakular/      Editor, EditorWidgets, TreeRenderer, Server (OAKULAR_WITH_SERVER=1)
applications/Oakular/  standalone app (GLFW window owned here, not in src/Oakular)
doc/examples/<Name>/   one program each -> build/Example-<Name>; run from repo root
tests/            GoogleTest, mirrors src/ (Nodes/, Builder/, Blackboard/, Core/)
benchmarks/       comparison with BehaviorTree.CPP (make benchmark)
BlackThorn.mk     how external projects (RobotIK) compile or link both layers
external/         third parties from external/manifest (do not edit)
```

## Commands

```bash
make download-external-libs      # once
make -j"$(nproc)"                # libs, Oakular, examples, benchmarks
make tests -j"$(nproc)"          # unit tests
make BLACKTHORN_WITH_NETWORK=0 OAKULAR_WITH_SERVER=0   # no SFML
```

## Rules that are easy to break

- A node is a map in YAML: `- Success:` (colon), never `- Success`.
- The YAML builder reads only the types of `getNodeCreators()` in
  `Builder.cpp`. `ReactiveSequence`, `SequenceWithMemory`, `ReactiveSelector`,
  `SelectorWithMemory` are C++ only today; `Parallel` needs thresholds or
  policies. Keep doc/03 §2 and doc/04 in sync when this changes.
- Node state lives in `Tree::Runtime` arrays indexed by `Node::index()`, not
  in node objects. Built-in behavior is a `case` in `Tree.cpp`.
- `Blackboard::get<T>` is strict: no conversion between `int` and `double`.
- Timed nodes read `Tree::now()` (`tree->clock(fn)`), never
  `steady_clock::now()` directly: simulators rely on it.
- A `CallbackLeaf` reset function runs on `reset()`, not on `halt()`;
  `onHalt()` is the halt hook (a `Timeout` halts its child).
- `oakular::Editor` never creates a window, a GL context or an ImGui context,
  never compiles ImGui sources, never calls GLFW. Ask the host with a signal.
- Network code stays behind `BLACKTHORN_HAS_NETWORK` / `OAKULAR_HAS_SERVER`.
- RobotIK consumes this repository as `external/BlackThorn` through
  `BlackThorn.mk`: do not break its variables.

## Style

- `.clang-format` (braces on their own line, 4 spaces).
- Parameters `p_name`, members `m_name`, constants `c_name` or `UPPER_CASE`.
- Doxygen `//!` comments; class banners `// ****...`, method banners `// ----...`.
- Every file starts with the `@file` / `@brief` / MIT header (copy a neighbour).
- Commit subject: imperative sentence, no prefix. Docs: simple English for
  French readers.
