---
name: blackthorn-tree
description: Write a BlackThorn behavior tree (YAML tree, Blackboard and SubTrees sections) and the C++ that runs it (NodeFactory, registerAction/registerCondition, CallbackLeaf with ports, Builder::fromFile, tick loop, simulated clock, halt/reset). Use for *.yaml trees, custom actions, "Failed to create X node", "Unknown node type", blackboard type problems.
---

# BlackThorn trees and custom nodes

YAML names the leaves; a `bt::NodeFactory` maps each name to C++; the
builder creates one node per occurrence; the app calls `tree->tick()`.

## YAML

```yaml
Blackboard:                     # optional, starting values (type from the text)
  battery: 80                   # int  (get<int>)
  speed: 1.0                    # double: write 1.0, not 1, to read get<double>
  dock: { x: 0.0, y: 0.0 }      # bt::BlackboardMap
BehaviorTree:                   # required, exactly one root node
  Selector:
    name: Root                  # free label; for Action/Condition = factory name
    children:                   # composites
      - Sequence:
          children:
            - Condition: { name: BatteryOk }
            - Timeout:
                milliseconds: 3000
                child:          # decorators: list with exactly one node
                  - Action: { name: Clean }
      - SubTree:
          reference: GoCharge   # required, key of SubTrees
          parameters: { target: ${dock} }   # existing parent key = input, missing = output
      - Success:                # a node is a map: colon required
SubTrees:
  GoCharge:
    Action: { name: MoveTo, parameters: { goal: ${target} } }
```

YAML types: `Sequence`, `Selector`, `Parallel` (needs
`success_threshold`/`failure_threshold` or `success_on_all`/`fail_on_all`),
`Inverter`, `ForceSuccess`, `ForceFailure`, `Repeat`/`Repeater` (`times`, 0 =
forever, port `repetitions`), `UntilSuccess`/`UntilFailure` (`attempts`),
`Timeout`/`Delay`/`Cooldown`/`Wait` (`milliseconds`, default 1000; Timeout
port `milliseconds`), `RunOnce`, `Action`, `Condition`, `Success`, `Failure`,
`SetBlackboard` (`key`, `value`: stored as string), `SubTree`. Reactive and
WithMemory composites: C++ only (`Unknown node type` in YAML).

Semantics to remember: `Sequence`/`Selector` resume at the RUNNING child and
do not re-tick earlier children (a condition before a long action is checked
once) → re-check inside the action, wrap the action in `Timeout`, or build a
`ReactiveSequence` in C++.

## C++

```cpp
#include "BlackThorn/BlackThorn.hpp"

auto blackboard = std::make_shared<bt::Blackboard>();
bt::NodeFactory factory;
factory.registerCondition("BatteryOk", [blackboard]() {
    return blackboard->getOrDefault<int>("battery", 0) > 20; });
factory.registerAction("Clean", [&robot]() { return robot.cleanStep(); }); // never block
factory.registerNode<MoveTo>("MoveTo", blackboard);       // class with ports
factory.registerNode("Skill", [&]() { return std::make_unique<bt::CallbackLeaf>(tickFn, resetFn); });

auto result = bt::Builder::fromFile(factory, "tree.yaml", blackboard); // or fromText
if (!result) { std::cerr << result.getError(); return 1; }
bt::Tree::Ptr tree = result.moveValue();
tree->clock([&]() { return sim_time_point; });            // timed nodes read this
while (running) { bt::Status s = tree->tick(); }
```

Node with ports: derive `bt::CallbackLeaf`, pass `[this]() {...}` and the
blackboard to its constructor; inside read `getInput<T>("port")`
(`std::optional`, strict type), write `setOutput("port", v)`.
Stop work: reset function (2nd ctor arg) runs on `tree->reset()` and when a
Repeat/Until re-runs the child; **override `onHalt()`** for `Timeout` and
`tree->halt()` (reset function is NOT called on halt).
Build without YAML: `Tree::create()`, `createRoot<T>()`, `addChild<T>()`,
`createChild<T>()` (decorators).
Blackboard: `set(key, v)` (let the type deduce; `set<int>(k, lvalue)` does
not compile), `get<T>`, `getOrDefault`, `has`, `createChild`, `dump()`.

## Builder errors

`Invalid node format: must be a map` (missing colon) · `Unknown node type: X`
· `Failed to create X node: X` (name not registered) · `Missing policies or
thresholds` (Parallel) · `Decorator must have exactly one child` · `Unknown
subtree reference: X`.

Reference: doc/03-YAML-Format.md, doc/04-Nodes.md, doc/05-Your-Own-Nodes.md,
doc/06-Blackboard.md. Examples: doc/examples/Patrol (ports, subtrees).
