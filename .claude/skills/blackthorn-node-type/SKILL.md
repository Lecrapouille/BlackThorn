---
name: blackthorn-node-type
description: Add, change or fix a built-in BlackThorn node type in the engine (a new decorator, composite or leaf such as a reactive composite, a new timed node, a YAML field), or make an existing C++-only node readable from YAML. Use for changes to Node::Kind, Tree.cpp runtime, Builder getNodeCreators, Exporter, visitors, the Oakular palette.
---

# Adding or changing a built-in node type

The runtime is data-oriented: a node object only carries its kind, name,
blackboard and ports; its behavior is a `case` in `Tree.cpp`, its state lives
in `Tree::Runtime` arrays indexed by `Node::index()`, its settings in
`NodeConfig` (filled by `fillConfig`). Users' own leaves are `CallbackLeaf`
(kind `Callback`) and need none of this.

## Touch points (follow an existing type, e.g. `Cooldown`, through all of them)

| # | File | What |
|---|------|------|
| 1 | `src/BlackThorn/Nodes/Node.hpp` | value in `enum class Node::Kind`; new `NodeConfig` field if the node has a setting |
| 2 | `src/BlackThorn/Nodes/{Composites,Decorators,Leaves}/*.hpp` | the class: `static constexpr char const* toString()` (= YAML key), constructor, `fillConfig`, `registrationKind`, both `accept`, `NodeKindTraits` specialisation |
| 3 | `src/BlackThorn/Nodes/Node.cpp` | `toString(Kind)`, `isComposite` / `isDecorator` switches |
| 4 | `src/BlackThorn/Nodes/Tree.cpp` | `setUpNode` (start timers, reset counters), `runNode` (the behavior), `resetNode` / `haltNodeInternal` if it holds state; `Tree.hpp` Runtime array if new state |
| 5 | `src/BlackThorn/Visitors/Visitor.hpp` | forward declaration + `visitX` in both visitor interfaces |
| 6 | `src/BlackThorn/Builder/Exporter.cpp` | both visitors (YAML writer and Mermaid), write every field back |
| 7 | `src/BlackThorn/Network/VisualizerClient.cpp` | its visitors (YAML sent to Oakular) |
| 8 | `src/BlackThorn/Builder/Builder.cpp` | `createX(...)` reading the YAML fields + entry in `getNodeCreators()` |
| 9 | `src/Oakular/Editor.cpp` | palette entry `{ "X", "Composite"/"Decorator"/"Leaf", has_ports }` |
| 10 | `src/Oakular/TreeRenderer.cpp` | color of the type |
| 11 | `tests/Nodes/...`, `tests/Builder/TestBuilder.cpp` | behavior tests (use `tree->clock(...)` for time, see `TestClock.cpp`) and a YAML round trip |
| 12 | `doc/04-Nodes.md`, `doc/03-YAML-Format.md` §2 list, `doc/08-API-Reference.md`, `blackthorn-tree` skill | document it |

A pure-virtual `visitX` breaks every visitor at compile time: that is the
checklist. RobotIK (`../../src/Robotik`) compiles these sources too.

## Rules

- Timed behavior reads `now()` of the tree (`Tree::now()`), never the wall clock.
- A composite/decorator that stops a RUNNING child must `haltNode(child)`
  (see `haltChildrenAfter` for the reactive composites); a halted
  `CallbackLeaf` gets `onHalt()` then its reset function.
- Never block in `runNode`; one `tick()` = one pass. Loops across ticks
  (`Repeat`, `UntilSuccess`) reset the child and return `RUNNING`.
- Validation of YAML fields belongs in the builder: return
  `Return<uint32_t>::error("...")` with a message naming the node and field.
- Keep the exporter and the builder symmetrical: what one writes, the other
  reads (Oakular saves through the same format).

Build and check: `make -j"$(nproc)" && make tests -j"$(nproc)"`.
