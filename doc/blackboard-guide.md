# 🧠 Blackboard Guide

The blackboard is a shared data store accessible by all nodes in the tree. It supports primitives (int, bool, string, double) and complex nested structures (maps, arrays).

---

## 🙊 Basic Usage

### 📝 YAML Syntax

```yaml
Blackboard:
  battery: 75
  target: "Drone-A"
  speed: 1.5
```

### ✍️ Setting Values

```cpp
auto blackboard = std::make_shared<bt::Blackboard>();
blackboard->set<int>("battery", 75);                // 🔋 Battery
blackboard->set<std::string>("target", "Drone-A");  // 🎯 Target
blackboard->set<double>("speed", 1.5);              // ⚡ Speed
```

### 🔍 Getting Values

```cpp
// battery is a std::optional<int>
auto battery = blackboard->get<int>("battery");
if (battery && *battery > 20) {
    std::cout << "✅ battery voltage: " << *battery << std::endl;
}

// 🛡️ Fallback with default value (100), battery_level is an integer.
int battery_level = blackboard->getOrDefault<int>("battery", 100);
```

---

## 🙈 Complex Structures

You can store your complex C++ structures in the blackboard. In this example, you have an array of two drones as enemies at different position and health.

### 📝 YAML Syntax

```yaml
Blackboard:
  game_state:
    score: 1280
    time: 42.5
    enemies:
      - name: "Drone-A"
        health: 35
        position: { x: 0.0, y: 5.0, z: 1.5 }
      - name: "Drone-B"
        health: 20
        position: { x: -3.5, y: 2.25, z: 0.0 }
```

### 🔍 Accessing in C++

Nested YAML maps and arrays are stored as `bt::BlackboardValue`, a variant covering the YAML types, with an `std::any` fallback for custom C++ types.

```cpp
using ValueMap = bt::BlackboardMap;                    // map of BlackboardValue
using ValueList = std::vector<bt::BlackboardValue>;    // heterogeneous array
using NumericList = std::vector<double>;               // array of numbers only

auto snapshot = blackboard->get<ValueMap>("game_state");
if (snapshot) {
    auto score = std::get_if<int>(&snapshot->at("score").asBase());
    auto enemies = std::get_if<ValueList>(&snapshot->at("enemies").asBase());
}
```

---

## 🔗 Variable References

Use `${key}` to reference existing data in YAML:

```yaml
Blackboard:
  primary_enemy:
    name: "Drone-A"
    health: 35
  selected_target: ${primary_enemy}  # Copy entire structure
```

---

## 🧩 Scoped Blackboards

Subtrees create child blackboards with `parameters:` that inherit from the parent:

```yaml
BehaviorTree:
  SubTree:
    name: ProcessEnemy
    reference: EngageEnemy
    parameters:
      target: ${primary_enemy}  # Passed to child blackboard

SubTrees:
  EngageEnemy:
    Sequence:
      children:
        - Action:
            name: AimWeapon
            parameters:
              enemy: ${target}  # Reads from child scope, not parent
```

---

## 🔄 Comparison with BehaviorTree.CPP

BlackThorn is inspired by [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) but uses a simpler, YAML-based approach.

| Aspect | BehaviorTree.CPP | BlackThorn |
|--------|------------------|------------|
| Tree definition format | XML | YAML |
| Variable reference syntax | `{key}` | `${key}` |
| Blackboard location | TreeNode (base) | Node (base) |
| getInput signature | `getInput<T>("port")` | `getInput<T>("port")` |
| setOutput signature | `setOutput("port", val)` | `setOutput("port", val)` |
| Port configuration | Via NodeConfig constructor | Via `setPortRemapping()` |
| Decorators with ports | Yes | Yes (Repeater, Timeout) |
| Thread-safe | Yes (mutex) | No |
| Scripting integration | Yes (Lua, custom) | No |
| SubTree port remapping | Yes | Yes |

### SubTree Port Remapping Example

BehaviorTree.CPP (XML):
```xml
<SubTree ID="MoveRobot" target="{move_goal}" result="{move_result}"/>
```

BlackThorn (YAML):
```yaml
SubTree:
  reference: MoveRobot
  parameters:
    target: ${move_goal}
    result: ${move_result}
```

---

## 👍 Best Practices

1. **Use descriptive keys**: `battery_level` instead of `b1`
2. **Type safety**: Always use the template parameter when getting values
3. **Check for existence**: Use optional return values or `has()` method
4. **Scoped access**: Use subtree parameters to explicitly pass data
5. **Avoid global state**: Prefer passing data through subtree parameters

For more examples, see [examples](examples/examples-guide.md).
