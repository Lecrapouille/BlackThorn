# 🧠 6. The blackboard

The blackboard is a dictionary (key → value) shared by the nodes of a tree. Nodes do not know each other: one writes `target`, another reads it. This page explains how to write values, which types you can read back, how `${key}` copies data, and how subtrees get their own scope.

---

## 📥 1. Write and read

In YAML, the `Blackboard` section gives the starting values:

```yaml
Blackboard:
  battery: 75
  target: "Drone-A"
  speed: 1.5
```

In C++:

```cpp
auto blackboard = std::make_shared<bt::Blackboard>();
blackboard->set("battery", 75);                       // 🔋 int
blackboard->set("target", std::string("Drone-A"));    // 🎯 std::string
blackboard->set("speed", 1.5);                        // ⚡ double

std::optional<int> battery = blackboard->get<int>("battery");
if (battery && *battery > 20)
{
    std::cout << "battery: " << *battery << std::endl;
}

int level = blackboard->getOrDefault<int>("battery", 100);  // 100 when missing
```

| Method | Does |
|--------|------|
| `set(key, value)` | writes (the type is the type of `value`) |
| `get<T>(key)` | `std::optional<T>`, empty when the key is missing **or has another type** |
| `getOrDefault<T>(key, default)` | the value, or `default` |
| `has(key)` | `true` when the key exists |
| `remove(key)` | removes a key of this scope |
| `keys()` | the keys of this scope |
| `dump()` | a text listing, to print while debugging |

> 💡 Let C++ deduce the type: `set("battery", battery)`. With an explicit type and a variable, `set<int>("battery", battery)` does not compile (the parameter is a forwarding reference).

Pass the blackboard to the builder so that the tree and your code share it: `bt::Builder::fromFile(factory, "tree.yaml", blackboard)`.

---

## 🔤 2. Types: what `get` can read

`get<T>` is **strict**: it returns the value only when it was stored with the type `T`. No conversion happens. Real results:

| Stored | `get<int>` | `get<double>` | `get<std::string>` |
|--------|-----------|---------------|--------------------|
| `set("battery", 75)` | ✅ `75` | ❌ empty | ❌ empty |
| `set("ratio", 0.5)` | ❌ empty | ✅ `0.5` | ❌ empty |

So in YAML, **write the type you will read**: `speed: 1.0` for a `double`, not `speed: 1`.

The YAML text decides the type:

```yaml
Blackboard:
  count: 42          # int    -> get<int>("count")
  ratio: 0.5         # double -> get<double>("ratio")
  ready: true        # bool   -> get<bool>("ready")
  version: "42"      # string -> get<std::string>("version")
  goal: "1;2;3"      # string, never the number 1
```

- Quotes make a string, as the YAML specification says.
- Only text that is a complete number or boolean becomes one: `1.2.3` and `3 apples` stay strings. `.inf`, `-.inf` and `.nan` are `double`.
- Saving a blackboard (Oakular, `bt::Exporter`) quotes the strings that could be read back as something else, so types survive a save and a load.

All the types a value can have (`bt::BlackboardValue`):

| C++ type | From YAML |
|----------|-----------|
| `int`, `double`, `bool`, `std::string` | scalars |
| `float`, `size_t` | C++ only |
| `bt::BlackboardMap` | a map: `{ x: 1.0, y: 2.0 }` |
| `std::vector<bt::BlackboardValue>` | a list |
| `std::vector<double>` | C++ only |
| any other copyable type | C++ only, stored in a `std::any`: `set("pose", Pose{ 1.0, 2.0 })` then `get<Pose>("pose")` |

---

## 🙈 3. Nested values

Maps and lists can be nested as deep as you want:

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

Read them with `std::get_if` on the variant:

```cpp
using ValueList = std::vector<bt::BlackboardValue>;

auto state = blackboard->get<bt::BlackboardMap>("game_state");
if (state)
{
    auto score = std::get_if<int>(&state->at("score").asBase());
    auto enemies = std::get_if<ValueList>(&state->at("enemies").asBase());
}
```

`doc/examples/GameState` prints such a structure.

### `${key}`: copy a value

In the `Blackboard` section, `${key}` copies another value, maps and lists included:

```yaml
Blackboard:
  primary_enemy:
    name: "Drone-A"
    health: 35
  selected_target: ${primary_enemy}   # 📋 a copy of the whole map
```

In `parameters`, `${key}` does not copy: it **connects** a port to the key ([chapter 3 §3](03-YAML-Format.md#-3-parameters-and-ports)).

---

## 🧩 4. Scopes: subtrees have their own blackboard

Each `SubTree` node gets a **child blackboard**:

```text
root blackboard        battery, dock, move_goal
   └── child of "GoCharge"   target (+ reads battery, dock… from its parent)
```

- The child **reads** the keys of its parent when it does not have them.
- What the child **writes** stays in the child.
- The `parameters` of the `SubTree` node are the only door between the two.

```yaml
- SubTree:
    reference: MoveRobot
    parameters:
      target: ${move_goal}     # input: the parent has "move_goal"
      result: ${move_result}   # output: the parent does not have it yet
```

| The parent key… | The port is… | What happens |
|-----------------|--------------|--------------|
| exists when the tree is built | an **input** | its value is copied into the child under the port name |
| does not exist yet | an **output** | after each tick of the subtree, the child value is written back to the parent key |
| (a literal, no `${}`) | a constant | the child gets this value |

`doc/examples/SubTreeRemapping` shows both directions.

### Compared with BehaviorTree.CPP

| | BehaviorTree.CPP | BlackThorn |
|---|---|---|
| Tree file | XML | YAML |
| Reference syntax | `{key}` | `${key}` |
| Subtree ports | `<SubTree ID="MoveRobot" target="{move_goal}"/>` | `SubTree: { reference: MoveRobot, parameters: { target: ${move_goal} } }` |
| Thread-safe blackboard | yes (mutex) | no: one tree, one thread |
| Scripting | yes | no |

---

## 👍 5. Good rules

1. **Clear key names**: `battery_level`, not `b1`.
2. **Write the type you read**: `1.0` for a `double`.
3. **Check what you read**: `get` returns an optional; test it.
4. **Use ports in nodes**, keys only in the `Blackboard` section and in `parameters`.
5. **Pass data to subtrees through `parameters`**, not through global keys.

Next: [07-Oakular.md](07-Oakular.md).
