# 📝 3. The YAML format

In this page you learn everything a BlackThorn file can hold: the tree, the starting values of the blackboard, and reusable subtrees. One file holds all three; Oakular reads and writes the same file.

All the examples of this page are loaded by `bt::Builder::fromFile` (or `fromText`): you can paste them in the tutorial of [chapter 2](02-Getting-Started.md#-4-tutorial-your-first-tree).

---

## 📚 1. The three sections

```yaml
# 🧠 Starting values of the blackboard (optional)
Blackboard:
  battery: 80
  dock: { x: 0.0, y: 0.0 }

# 🌳 The tree that runs (required)
BehaviorTree:
  Selector:
    children: [...]

# 🔁 Trees that the main tree can call (optional)
SubTrees:
  GoCharge:
    Action:
      name: MoveTo
```

| Section | Required | Holds |
|---------|----------|-------|
| `BehaviorTree` | yes | exactly one node: the root |
| `Blackboard` | no | keys and values written to the root blackboard before the first tick ([chapter 6](06-Blackboard.md)) |
| `SubTrees` | no | named trees, called by a `SubTree` node ([§5](#-5-subtrees)) |

---

## 🌳 2. How a node is written

A node is a map with **one key**, its type. Under the type come its fields:

```yaml
Sequence:              # the type
  name: Work           # optional: a name shown in Oakular and in errors
  _id: 3               # optional: a number for the visualizer (§4)
  children:            # the children of a composite
    - Condition:
        name: BatteryOk
    - Action:
        name: Clean
```

| Field | Used by | Meaning |
|-------|---------|---------|
| `name` | every node | For `Action` and `Condition`: the name registered in the `NodeFactory` (required). For other nodes: a free label. |
| `_id` | every node | Number used by the visualizer. Given by the builder when missing. |
| `children` | composites | The list of children, ticked in this order. |
| `child` | decorators | A list with **exactly one** child. |
| `parameters` | `Action`, `Condition`, `SubTree`, `Repeat` | The ports of the node ([§3](#-3-parameters-and-ports)). |

A decorator writes its only child as a one-element list:

```yaml
Timeout:
  milliseconds: 3000
  child:
    - Action:
        name: Clean
```

A node without field still needs the colon, because a node is always a map:

```yaml
children:
  - Success:     # ✅ correct
  - Failure:     # ✅ correct
  - Success      # ❌ "Invalid node format: must be a map"
```

### Types you can write in YAML

| Family | Types |
|--------|-------|
| Composites | `Sequence`, `Selector`, `Parallel` |
| Decorators | `Inverter`, `ForceSuccess`, `ForceFailure`, `Repeat` (or `Repeater`), `UntilSuccess`, `UntilFailure`, `Timeout`, `Delay`, `Cooldown`, `RunOnce` |
| Leaves | `Action`, `Condition`, `Success`, `Failure`, `Wait`, `SetBlackboard` |
| Other | `SubTree` |

> ⚠️ `ReactiveSequence`, `SequenceWithMemory`, `ReactiveSelector` and `SelectorWithMemory` exist in C++ but the builder does not read them yet: a file that uses them fails with `Unknown node type: ReactiveSequence`. Build them in C++ ([chapter 5](05-Your-Own-Nodes.md#-7-build-a-tree-in-c-without-yaml)).

The fields of each type are listed in [chapter 4](04-Nodes.md).

---

## 🔗 3. Parameters and ports

`parameters` connects the **ports** of a node (its named inputs and outputs) to values:

```yaml
Action:
  name: MoveTo
  parameters:
    goal: ${dock}        # a reference: the port reads the blackboard key "dock"
    speed: 0.5           # a literal: the port reads 0.5
```

| Value | The port… |
|-------|-----------|
| `${key}` | reads and writes the blackboard key `key` |
| anything else | reads this constant |

In C++, the node reads its port with `getInput<double>("speed")` and writes with `setOutput("result", value)`: it never knows the key behind it ([chapter 5](05-Your-Own-Nodes.md#-3-ports-read-and-write-the-blackboard)). This is what makes a node reusable: two `MoveTo` nodes can go to two different places.

> 📌 The builder also copies a literal parameter into the blackboard of the node, under the port name (`speed` above). Two nodes of the same scope that use the same literal name share this key.

---

## 🔢 4. Node ids (`_id`) and the visualizer

The visualizer must know which node of the running program is which box on its screen. Each node has a number, `_id`:

```yaml
BehaviorTree:
  Sequence:
    _id: 1
    name: Root
    children:
      - Action:
          _id: 2
          name: Task1
```

- 🔢 The ids are plain integers.
- 🤖 When `_id` is missing, the builder gives one.
- 📤 `bt::Exporter` and Oakular always write `_id` back to the file.
- 👁️ The visualizer updates the boxes by id ([chapter 7](07-Oakular.md#-4-the-protocol)).

---

## 🔁 5. SubTrees

A subtree is a tree with a name, written under `SubTrees`, and called by a `SubTree` node:

```yaml
BehaviorTree:
  Selector:
    children:
      - Action:
          name: Clean
      - SubTree:
          name: GoCharge          # label of this call
          reference: GoCharge     # which definition to run (required)
          parameters:
            target: ${dock}       # "target" inside = "dock" outside

SubTrees:
  GoCharge:
    Action:
      name: MoveTo
      parameters:
        goal: ${target}           # reads the key of the subtree scope
```

Each call creates a **child blackboard**: the subtree sees the keys of its parent, but what it writes stays inside, except through its `parameters`. [Chapter 6](06-Blackboard.md#-4-scopes-subtrees-have-their-own-blackboard) explains inputs and outputs in detail.

---

## 🚨 6. Errors of the builder

The builder stops at the first error and returns its message (`result.getError()`). The ones you will meet:

| Message | Cause |
|---------|-------|
| `Invalid node format: must be a map` | a node written without colon (`- Success`) |
| `Unknown node type: X` | a typo in a type, or a type not readable from YAML (§2) |
| `Failed to create X node: X` | an `Action` or `Condition` whose `name` is not registered in the `NodeFactory` |
| `X node missing 'name' field` | an `Action` or `Condition` without `name` |
| `Node 'X' missing 'children' field` | a composite without `children` (or a decorator without `child`) |
| `Decorator must have exactly one child` | a decorator with zero or two children |
| `Missing policies or thresholds` | a `Parallel` without `success_threshold`/`failure_threshold` nor `success_on_all`/`fail_on_all` |
| `Unknown subtree reference: X` | a `SubTree` whose `reference` is not under `SubTrees` |

---

## 👉 7. Where to go next

- [04-Nodes.md](04-Nodes.md): the fields and the behavior of every type.
- [06-Blackboard.md](06-Blackboard.md): the `Blackboard` section, the types of values, `${key}` and scopes.
