# 🔨 5. Your own nodes

In this page you write the leaves that talk to your robot or your game: actions and conditions in C++. You learn to read and write the blackboard through ports, to stop a long action cleanly, to give the tree a simulated clock, and to build a tree without YAML.

Every program of this page was compiled and run; the outputs are real.

---

## 🧱 1. The factory: from a name to C++ code

The YAML file only names the leaves (`Action: { name: Clean }`). A `bt::NodeFactory` maps each name to the C++ code that creates the node. The builder asks the factory for one **new node per occurrence** of the name in the file.

| Call | Creates | Use it for |
|------|---------|------------|
| `registerAction(name, function)` | an action from a function returning a `bt::Status` | small actions |
| `registerCondition(name, function)` | a condition from a function returning a `bool` | yes/no questions |
| `registerNode<T>(name)` | `new T()` | your own node class |
| `registerNode<T>(name, blackboard)` | `new T(blackboard)` | a class that reads ports |
| `registerNode(name, creator)` | whatever `creator()` returns | a class that needs your own objects |

A name that the factory does not know stops the builder: `Failed to create X node: X`.

---

## ⚡ 2. Actions and conditions from functions

The quickest way. The function of an action returns a status; the function of a condition returns `true` or `false`. Capture what you need (your robot, your game state) in the lambda:

```cpp
auto blackboard = std::make_shared<bt::Blackboard>();

bt::NodeFactory factory;
factory.registerCondition("BatteryOk", [blackboard]() {
    return blackboard->getOrDefault<int>("battery", 0) > 20;
});
factory.registerAction("Clean", [blackboard]() {
    int battery = blackboard->getOrDefault<int>("battery", 0) - 30;
    blackboard->set("battery", battery);
    std::cout << "  Clean: battery " << battery << "%\n";
    return bt::Status::RUNNING;      // cleaning never ends by itself
});
```

### A long action: return `RUNNING`

An action must return quickly. If the work takes time, start it, return `RUNNING`, and check it at the next ticks:

```cpp
int steps = 0;
factory.registerAction("Walk", [&steps]() {
    ++steps;
    return (steps < 3) ? bt::Status::RUNNING : bt::Status::SUCCESS;
});
```

The rule: **never block** in an action (no `sleep`, no waiting loop). The whole tree runs in the thread that calls `tick()`.

---

## 🔌 3. Ports: read and write the blackboard

A node that reads `blackboard->get("dock")` only works when the key is called `dock`. A **port** removes this link: the node reads a port named `goal`, and the YAML file says which key (or which constant) is behind it.

```yaml
- Action:
    name: MoveTo
    parameters:
      goal: ${dock}      # the port "goal" reads the key "dock"
```

A node that uses ports is a class derived from `bt::CallbackLeaf` (the C++ class of `Action`). It reads with `getInput<T>(port)` and writes with `setOutput(port, value)`:

```cpp
//! A custom action with an input port: "goal" is remapped by the YAML.
class MoveTo final: public bt::CallbackLeaf
{
public:

    explicit MoveTo(bt::Blackboard::Ptr p_blackboard)
        : CallbackLeaf(
              [this]()
              {
                  auto goal = getInput<bt::BlackboardMap>("goal");
                  if (!goal)
                  {
                      return bt::Status::FAILURE;
                  }
                  std::cout << "  MoveTo: going to the dock\n";
                  return bt::Status::SUCCESS;
              },
              p_blackboard)
    {
    }
};

factory.registerNode<MoveTo>("MoveTo", blackboard);
```

| Method | Returns |
|--------|---------|
| `getInput<T>("port")` | `std::optional<T>`: empty when the key is missing or has another type |
| `setOutput("port", value)` | writes to the key behind the port (or to a key named like the port when there is no `parameters` entry) |

A map of the YAML file (`{ x: 0.0, y: 0.0 }`) is read as a `bt::BlackboardMap`; the types are listed in [chapter 6](06-Blackboard.md#-2-types-what-get-can-read).

> 💡 The builder gives the node the blackboard of its scope (the subtree blackboard inside a subtree). This is why `registerNode<MoveTo>("MoveTo", blackboard)` works inside subtrees too.

---

## 🧰 4. Give your own objects to a node

A real action needs your robot, your scheduler or your game world. Register a **creator** that builds the node with them. RobotIK does this to turn each of its skills into an action (`src/Robotik/Skills/SkillNodes.cpp`):

```cpp
p_factory.registerNode(
    p_scheduler.name(id),
    [&p_scheduler, id]()
    {
        // One flag per tree node: the same skill may appear twice.
        auto active = std::make_shared<bool>(false);
        return std::make_unique<bt::CallbackLeaf>(
            [&p_scheduler, id, active]()
            { return tickSkill(p_scheduler, id, *active); },
            [&p_scheduler, id, active]()
            {
                if (*active)
                {
                    p_scheduler.cancel(id);
                    *active = false;
                }
            });
    });
```

The creator runs once per node of the file: put the per-node state (here `active`) inside it, not outside.

---

## 🛑 5. Stop the work: reset and halt

A long action may be stopped before it ends. Two different events can stop it, and your node must handle both if it drives something real:

| Event | Who triggers it | What BlackThorn calls |
|-------|-----------------|-----------------------|
| **halt** | a `Timeout` whose time is over; `tree->halt()` | the virtual `onHalt()` of the node |
| **reset** | `tree->reset()`; a `Repeat`, `UntilSuccess` or `UntilFailure` that runs its child again | the reset function given to `CallbackLeaf` |

> ⚠️ The reset function is **not** called on a halt. A node that only gives a reset function keeps working after a `Timeout` stopped it. Override `onHalt()` too.

```cpp
//! A long action that stops its motor when it is halted or reset.
class Drive final: public bt::CallbackLeaf
{
public:

    Drive()
        : CallbackLeaf([]() { return bt::Status::RUNNING; },   // never ends
                       []() { std::cout << "  reset: stop the motor\n"; })
    {
    }

protected:

    void onHalt() override
    {
        std::cout << "  halt: stop the motor\n";
    }
};
```

Under a `Timeout` of 2 s, with a clock that moves 1 s per tick (see §6), then a `tree->reset()`:

```text
tick 1:
  tree -> RUNNING
tick 2:
  tree -> RUNNING
tick 3:
  halt: stop the motor
  tree -> FAILURE
tree->reset():
  reset: stop the motor
```

---

## ⏰ 6. Time: a simulated clock

`Timeout`, `Delay`, `Cooldown` and `Wait` read the clock of the tree. By default it is the wall clock (`std::chrono::steady_clock`). A simulator gives its own clock, so that a test gives the same result whether the computer is fast or slow:

```cpp
std::chrono::steady_clock::time_point now{};
tree->clock([&now]() { return now; });

// in the simulation loop:
now += std::chrono::seconds(1);   // or the simulated step
```

Subtrees take the clock of their parent when they start. An empty function (`tree->clock({})`) gives the wall clock back.

### Tutorial: the cleaning robot

This tree cleans while the battery is OK, and goes to the charger otherwise:

```yaml
Blackboard:
  battery: 80
  dock: { x: 0.0, y: 0.0 }

BehaviorTree:
  Selector:
    name: Root
    children:
      - Sequence:
          name: Work
          children:
            - Condition:
                name: BatteryOk
            - Timeout:
                milliseconds: 3000
                child:
                  - Action:
                      name: Clean
      - SubTree:
          name: GoCharge
          reference: GoCharge
          parameters:
            target: ${dock}

SubTrees:
  GoCharge:
    Action:
      name: MoveTo
      parameters:
        goal: ${target}
```

With the nodes of §2 and §3, and a clock moving 1 s per tick:

```cpp
auto result = bt::Builder::fromFile(factory, "robot.yaml", blackboard);
if (!result)
{
    std::cerr << "Error: " << result.getError() << std::endl;
    return 1;
}
bt::Tree::Ptr tree = result.moveValue();

// A simulated clock: 1 second per tick, whatever the real time.
std::chrono::steady_clock::time_point now{};
tree->clock([&now]() { return now; });

for (int tick = 1; tick <= 4; ++tick)
{
    std::cout << "tick " << tick << ":\n";
    bt::Status status = tree->tick();
    std::cout << "  tree -> " << bt::to_string(status) << "\n";
    now += std::chrono::seconds(1);
}
```

The real output:

```text
tick 1:
  Clean: battery 50%
  tree -> RUNNING
tick 2:
  Clean: battery 20%
  tree -> RUNNING
tick 3:
  Clean: battery -10%
  tree -> RUNNING
tick 4:
  MoveTo: going to the dock
  tree -> SUCCESS
```

🔍 Look at tick 3: the battery is at -10 % and the robot still cleans. The `Sequence` resumes at the running `Clean` and never asks `BatteryOk` again ([chapter 4](04-Nodes.md#sequence)). Only the `Timeout` stops it, at tick 4, and the `Selector` goes to the charger. Two fixes:

- re-check the battery inside `Clean` (return `FAILURE` when it is low), or
- use a `ReactiveSequence`, which asks `BatteryOk` at every tick (C++ only, §7).

---

## 🌲 7. Build a tree in C++, without YAML

A tree can be built in code: `Tree::create()`, then `createRoot<T>(...)`, then `addChild<T>(...)` on composites (or `createChild<T>(...)` on decorators). The arguments go to the constructor of `T` ([chapter 4 §5](04-Nodes.md#-5-the-same-nodes-in-c)). This is also the only way to use the reactive composites today:

```cpp
int battery = 80;

bt::Tree::Ptr tree = bt::Tree::create();
auto& root = tree->createRoot<bt::ReactiveSequence>();
root.addChild<bt::Condition>([&battery]() { return battery > 20; });
root.addChild<bt::CallbackLeaf>([&battery]() {
    battery -= 30;
    std::cout << "  Clean: battery " << battery << "%\n";
    return bt::Status::RUNNING;
});

for (int tick = 1; tick <= 4; ++tick)
{
    bt::Status status = tree->tick();
    std::cout << "tick " << tick << " -> " << bt::to_string(status) << "\n";
}
```

```text
  Clean: battery 50%
tick 1 -> RUNNING
  Clean: battery 20%
tick 2 -> RUNNING
tick 3 -> FAILURE
tick 4 -> FAILURE
```

This time the condition is asked at every tick: at tick 3 the battery is too low and the tree fails before cleaning again.

---

## ✅ 8. Good rules for a node

- **Never block.** Start the work, return `RUNNING`, check it later.
- **Stop what you started.** Override `onHalt()` and give a reset function when your node drives something real.
- **Read ports, not keys.** `getInput("goal")` keeps the node reusable; the file chooses the key.
- **Keep per-node state in the creator** (or in the node object), not in a global: the same name can appear twice in a tree.
- **Use the tree clock** for time, never `std::chrono` directly, so that a simulation is reproducible.
- **A condition answers at once.** If the answer needs time, it is an action.

Next: [06-Blackboard.md](06-Blackboard.md).
