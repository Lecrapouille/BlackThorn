# 📝 YAML Format Guide

BlackThorn uses YAML files to describe behavior trees, subtrees, and blackboard values. This guide explains the complete YAML format.

---

## 📚 File Structure

Every YAML file must contain the `BehaviorTree:` section and can contain two optional sections: `Blackboard:` and `SubTrees:`.

```yaml
# 🧠 Initial scoped keys available to all nodes
Blackboard:
  counter: 42
  position:
    x: 1.0
    y: 2.0
    z: 0.5

# 🌳 Root tree definition
BehaviorTree:
  Sequence:
    children: [...]

# 🔁 Reusable subtrees with isolated scopes
SubTrees:
  PatrolLoop:
    Sequence:
      children: [...]
```

---

## ⚙️ BehaviorTree Configuration

Each node can have:

- 🔢 `_id`: Unique numeric identifier for the node (auto-generated if not provided)
- 🏷️ `name`: A user-defined name for the node
- 🎛️ `parameters`: Input/output ports for blackboard access (for Action, Condition, SubTree nodes)
- 🌱 `children`: List of child nodes (for composite nodes)
- 🌿 `child`: Single child node (for decorator nodes)

📌 See [Node Types Guide](nodes-guide.md) for detailed syntax for each node type.

---

## 🔢 Node IDs with `_id`

Each node can have an optional `_id` field that uniquely identifies it. This ID is used by the visualization protocol to track node states efficiently.

```yaml
BehaviorTree:
  Sequence:
    _id: 1
    name: Root
    children:
      - Action:
          _id: 2
          name: Task1
      - Selector:
          _id: 3
          children:
            - Action:
                _id: 4
                name: Task2
```

**Key points:**

- 🔢 IDs are simple integers (1, 2, 3, ...)
- 🤖 If `_id` is not specified, the Builder auto-generates one
- 📤 The Exporter always includes `_id` in the output YAML
- 👁️ The visualizer uses these IDs to track state changes efficiently

---

## ⏱️ Temporal Nodes

BlackThorn provides several temporal nodes with `milliseconds` parameters:

```yaml
# Timeout: fails if child doesn't complete in time
- Timeout:
    _id: 10
    milliseconds: 5000
    child:
      - Action:
          _id: 11
          name: "LongTask"

# Delay: waits before starting child
- Delay:
    _id: 20
    milliseconds: 2000
    child:
      - Action:
          _id: 21
          name: "DelayedAction"

# Cooldown: prevents re-execution for a period
- Cooldown:
    _id: 30
    milliseconds: 3000
    child:
      - Action:
          _id: 31
          name: "RateLimitedAction"

# Wait: simple wait leaf
- Wait:
    _id: 40
    milliseconds: 1000

# RunOnce: executes child only once
- RunOnce:
    _id: 50
    child:
      - Action:
          _id: 51
          name: "OneTimeInit"
```

---

## 📝 SetBlackboard Node

Write values to the blackboard during tree execution:

```yaml
- SetBlackboard:
    _id: 60
    key: "target_found"
    value: "true"
```


---

## 📖 Blackboard Section

Keys defined under `Blackboard` populate the root blackboard. Complex structures are fully supported: nested maps, arrays, booleans, strings, and numbers. All nodes in the tree can access these values via `blackboard->get<T>("key")`. 🗝️

```yaml
Blackboard:
  counter: 42
  position:
    x: 1.0
    y: 2.0
    z: 0.5
  enemies:
    - name: "Drone-A"
      health: 35
    - name: "Drone-B"
      health: 20
```

---

## 🔗 Variable References with `${key}`

The `${key}` syntax allows you to reference any blackboard value by name, copying it into another field. This works with primitives, maps, and entire nested structures: 🪄

```yaml
Blackboard:
  primary_enemy:
    name: "Drone-A"
    health: 35
  selected_target: ${primary_enemy}  # 📋 Copies the entire enemy map
```

---

## 🛡️ SubTree Scope Isolation

When a `SubTree` node is instantiated, the builder creates a child blackboard (`parent->createChild()`) for that subtree. Parameters passed to the subtree are stored in this child scope, preventing data leakage into the parent. The child blackboard inherits all parent keys but can override them locally. 🔒🌳

```yaml
BehaviorTree:
  SubTree:
    name: ProcessTarget
    reference: EngageEnemy
    parameters:
      target: ${primary_enemy}  # 🎯 Passed to child blackboard

SubTrees:
  EngageEnemy:
    Sequence:
      children:
        - Action:
            name: AimWeapon
            parameters:
              enemy: ${target}  # 👁️ Reads from child scope
```
