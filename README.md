# 🌳 BlackThorn — a C++ behavior tree library

![blackthorn](doc/logos/blackthorn.png)

> ⚠️ The API is still evolving. Expect breaking changes while the library matures.

**BlackThorn** is a C++17 behavior tree library for robotics, simulation and games. It comes with **Oakular**, an editor and live visualizer that you can run alone or embed in your own application.

A **behavior tree** decides what a robot or a game character does next. You build a big behavior from small bricks (*open the door*, *walk*, *is the battery OK?*) without each brick knowing the others. Unlike a state machine, adding a behavior does not add arrows everywhere. New to behavior trees? Read [the introduction](doc/01-Introduction.md) first: ten minutes, no code.

```yaml
BehaviorTree:                       # tree.yaml
  Selector:
    children:
      - Sequence:
          children:
            - Condition: { name: BatteryOk }
            - Timeout:
                milliseconds: 3000
                child:
                  - Action: { name: Clean }
      - Action: { name: GoCharge }
```

```cpp
bt::NodeFactory factory;
factory.registerCondition("BatteryOk", [&]() { return battery > 20; });
factory.registerAction("Clean", [&]() { return robot.clean(); });     // RUNNING, SUCCESS or FAILURE
factory.registerAction("GoCharge", [&]() { return robot.goCharge(); });

auto result = bt::Builder::fromFile(factory, "tree.yaml");
bt::Tree::Ptr tree = result.moveValue();
while (running) { tree->tick(); }   // once per cycle of your main loop
```

---

## ✨ Why BlackThorn?

- 📝 **One YAML file** holds the tree, the starting blackboard and the subtrees. Oakular reads and writes the same file.
- 🧠 **A real blackboard**: nested maps and lists, `${key}` references, one scope per subtree, ports remapped in the file.
- 👁️ **Oakular**, an open-source editor and live visualizer: draw a tree, or watch the nodes of a running program turn green, yellow and red.
- 🧩 **Embeddable**: the engine has no graphical dependency; the editor is a plain Dear ImGui component that never opens a window. RobotIK puts it in its simulator.
- ⏰ **Simulated time**: `Timeout`, `Delay`, `Cooldown` and `Wait` read the clock you give, so a simulation gives the same result fast or slow.
- ⚡ **Data-oriented runtime**: the state of the nodes is stored in arrays, not scattered in objects. `make benchmark` compares it with BehaviorTree.CPP.

BlackThorn started from [BrainTree](https://github.com/arvidsson/BrainTree) and adds what it misses (blackboard, files, subtrees, editor), while staying smaller and easier to read than [BehaviorTree.CPP](https://www.behaviortree.dev/) (XML in two files, editor not fully open source). [Chapter 1](doc/01-Introduction.md#-5-why-blackthorn) compares the three.

*BlackThorn* plays with "BT"; the blackthorn is a thorny tree (*prunellier* or *épine noire* in French). *Oakular* mixes "oak" and "ocular".

---

## 🚀 Quick start

```bash
# Ubuntu: sudo apt-get install libsfml-dev libglfw3-dev libglew-dev
# Fedora: sudo dnf install SFML-devel glfw-devel glew-devel
git clone https://github.com/Lecrapouille/BlackThorn --recurse-submodules
cd BlackThorn
make download-external-libs
make -j"$(nproc)"                  # library, Oakular, examples, benchmarks

./build/Oakular                    # the editor (Mode → Visualizer to watch a tree)
./build/Example-Patrol             # a tree with subtrees, sent to Oakular
```

Your first tree, step by step: [doc/02-Getting-Started.md](doc/02-Getting-Started.md). Use BlackThorn in your project (with [BlackThorn.mk](BlackThorn.mk)): [doc/07-Oakular.md](doc/07-Oakular.md#-5-embed-oakular-in-your-application).

---

## 📚 Documentation

| # | Chapter | Contents |
|---|---------|----------|
| | [doc/README.md](doc/README.md) | Reading order and quick paths |
| 1 | [Introduction](doc/01-Introduction.md) | 🌱 Behavior trees in ten minutes: tick, status, nodes, blackboard |
| 2 | [Getting started](doc/02-Getting-Started.md) | 🚀 Build, run the examples, write your first tree |
| 3 | [The YAML format](doc/03-YAML-Format.md) | 📝 Tree, blackboard and subtrees in one file |
| 4 | [The nodes](doc/04-Nodes.md) | 🎯 Every built-in node and when to use it |
| 5 | [Your own nodes](doc/05-Your-Own-Nodes.md) | 🔨 Actions, conditions, ports, halt and reset, simulated clock |
| 6 | [The blackboard](doc/06-Blackboard.md) | 🧠 Types, nested values, scopes of subtrees |
| 7 | [Oakular](doc/07-Oakular.md) | 👁️ Editor, live visualizer, protocol, embedding |
| 8 | [API reference](doc/08-API-Reference.md) | 📖 The classes, by task |
| 9 | [Examples](doc/09-Examples.md) | 🌟 What each example shows |
| | [src/Oakular/README.md](src/Oakular/README.md) | 🖥️ The `oakular::Editor` API |

Coding agents: [AGENTS.md](AGENTS.md) and the skills of `.claude/skills/`.

---

## 🔗 Read more

- [Introduction to behavior trees](https://roboticseabass.com/2021/05/08/introduction-to-behavior-trees/) – roboticseabass.com
- [Behavior trees in detail](https://lisyarus.github.io/blog/posts/behavior-trees.html) – lisyarus.github.io
- [BehaviorTree.CPP documentation](https://www.behaviortree.dev/) – a related library with the same ideas

---

## 📝 License

MIT License — see [LICENSE](LICENSE).
