# 🚀 2. Getting started

In this page you build BlackThorn, run the examples and the Oakular editor, then write your first tree: a robot that crosses a room. It takes about 20 minutes (most of it is compilation).

---

## 📦 1. Install the prerequisites

BlackThorn builds on **Linux** with `make` (no CMake) and a C++17 compiler. The engine itself only needs [rapidyaml](https://github.com/biojppm/rapidyaml) (downloaded for you). The other libraries are for the network link and the Oakular application.

| Library | Used by |
|---------|---------|
| SFML network | the live visualizer (`bt::VisualizerClient`, `oakular::Server`) |
| GLFW, GLEW, OpenGL | the Oakular application window |
| GoogleTest | the unit tests only |

Ubuntu / Debian:

```bash
sudo apt-get install libsfml-dev libglfw3-dev libglew-dev libgtest-dev libgmock-dev
```

Fedora:

```bash
sudo dnf install SFML-devel glfw-devel glew-devel gtest-devel gmock-devel
```

You do not want SFML at all? Build with `make BLACKTHORN_WITH_NETWORK=0 OAKULAR_WITH_SERVER=0`: you lose only the live visualizer ([chapter 7](07-Oakular.md#-6-build-switches)).

---

## 🔨 2. Build

```bash
git clone https://github.com/Lecrapouille/BlackThorn --recurse-submodules
cd BlackThorn
make download-external-libs   # once: Dear ImGui, ImGuiFileDialog, rapidyaml, ...
make -j"$(nproc)"             # the libraries, Oakular, the examples, the benchmarks
```

`--recurse-submodules` fetches `.makefile`, the build system. Without it, `make` has no rules.

After the build, `build/` holds:

| Path | What it is |
|------|------------|
| `build/Oakular` | the editor and live visualizer |
| `build/Example-*` | one program per folder of `doc/examples/` |
| the static archives of `bt::` and `oakular::` | to link in your own project ([chapter 7](07-Oakular.md#-5-embed-oakular-in-your-application)) |

The CI also publishes Oakular as a web page: [Oakular.html](https://lecrapouille.github.io/BlackThorn/Oakular.html). File > Open starts in `doc/examples`. The live visualizer stays on the desktop.

Unit tests, for developers:

```bash
make tests -j"$(nproc)"
```

---

## 🏃 3. Run the examples

Run them from the root of the repository: they read their YAML file with a path such as `doc/examples/Patrol/Patrol.yaml`.

```bash
./build/Example-Nodes sequence    # one node family: leaf, sequence, selector, parallel, decorator
./build/Example-Patrol            # subtrees and ports, sent to the visualizer
./build/Example-GameState         # nested blackboard values
./build/Example-StatusDemo        # random successes and failures, to watch in Oakular
./build/Example-SubTreeRemapping  # inputs and outputs of a subtree
./build/Example-Embedded          # Oakular inside a host window
```

[Chapter 9](09-Examples.md) explains what each one shows.

### 🔭 Watch a tree live

1. Start the editor: `./build/Oakular`.
2. In the menu, choose **Mode → Visualizer**. Oakular waits for a program on TCP port 8888 (another port: `./build/Oakular --port 9090`, and the same port in `connect`).
3. In another terminal, run `./build/Example-Patrol`. It connects, sends its tree, then the status of each node after every tick. The nodes turn green, yellow and red as the tree runs.

---

## 🚪 4. Tutorial: your first tree

A robot must cross a room: open the door, walk, close the door. Walking takes three ticks.

```text
        Sequence  "CrossTheRoom"
       /    |     \
 OpenDoor  Walk  CloseDoor
```

### Step 1 — the tree, in YAML

Create `door.yaml`:

```yaml
BehaviorTree:
  Sequence:
    name: CrossTheRoom
    children:
      - Action:
          name: OpenDoor
      - Action:
          name: Walk
      - Action:
          name: CloseDoor
```

The file only gives **names** to the actions. It does not say what `OpenDoor` does: that is C++, in your program.

### Step 2 — the actions, in C++

Create `main.cpp`:

```cpp
#include "BlackThorn/BlackThorn.hpp"
#include <iostream>

int main()
{
    // 1. Tell BlackThorn what each action name does.
    bt::NodeFactory factory;
    factory.registerAction("OpenDoor", []() {
        std::cout << "open the door\n";
        return bt::Status::SUCCESS;
    });
    int steps = 0;
    factory.registerAction("Walk", [&steps]() {
        std::cout << "walk, step " << ++steps << "\n";
        return (steps < 3) ? bt::Status::RUNNING : bt::Status::SUCCESS;
    });
    factory.registerAction("CloseDoor", []() {
        std::cout << "close the door\n";
        return bt::Status::SUCCESS;
    });

    // 2. Build the tree from the YAML file.
    auto result = bt::Builder::fromFile(factory, "door.yaml");
    if (!result)
    {
        std::cerr << "Error: " << result.getError() << std::endl;
        return 1;
    }
    bt::Tree::Ptr tree = result.moveValue();

    // 3. Tick the tree until it stops running.
    bt::Status status = bt::Status::RUNNING;
    while (status == bt::Status::RUNNING)
    {
        status = tree->tick();
        std::cout << "tree: " << bt::to_string(status) << "\n";
    }
    return 0;
}
```

Three steps, always the same:

1. A **`NodeFactory`** maps each name of the YAML file to C++ code. `registerAction` takes a function that returns a status.
2. **`Builder::fromFile`** reads the file and builds the tree. It returns a result: test it, and read `getError()` when it failed.
3. Your program calls **`tick()`**. Here a `while` loop; in a robot or a game, once per cycle of your main loop.

### Step 3 — run it

Put `door.yaml` and `main.cpp` in a new folder `first-tree/` of your clone. The quickest build compiles the BlackThorn sources with your file (no network, so no SFML), and links the rapidyaml archive that `make` built:

```bash
g++ -std=c++17 first-tree/main.cpp \
    $(find src/BlackThorn -name '*.cpp' -not -path '*/Network/*') \
    -I src -isystem external/rapidyaml/src -isystem external/rapidyaml/ext/c4core/src \
    external/rapidyaml/build/libryml.a -o first-tree/door
cd first-tree && ./door
```

For a real project, use [BlackThorn.mk](../BlackThorn.mk) instead ([chapter 7](07-Oakular.md#-5-embed-oakular-in-your-application) shows both ways).

The real output:

```text
open the door
walk, step 1
tree: RUNNING
walk, step 2
tree: RUNNING
walk, step 3
close the door
tree: SUCCESS
```

Read it carefully:

- Tick 1: `OpenDoor` succeeds, so the `Sequence` goes on to `Walk`, which returns `RUNNING`. The `Sequence` stops there and returns `RUNNING`.
- Tick 2: the `Sequence` **resumes at `Walk`**. It does not open the door again.
- Tick 3: `Walk` succeeds, `CloseDoor` succeeds, the tree succeeds.

### Step 4 — break it on purpose

Make `OpenDoor` return `bt::Status::FAILURE` and run again:

```text
open the door
tree: FAILURE
```

A `Sequence` fails as soon as one child fails: the robot does not walk into a closed door. Now misspell a name in the YAML file (`OpenDor`):

```text
Error: Failed to create OpenDor node: OpenDor
```

The builder refuses a name that the factory does not know: the tree never starts with a hole in it.

---

## 👉 5. Where to go next

- [03-YAML-Format.md](03-YAML-Format.md): everything a tree file can hold.
- [04-Nodes.md](04-Nodes.md): the other nodes (`Selector`, `Timeout`, `Repeat`…).
- [05-Your-Own-Nodes.md](05-Your-Own-Nodes.md): actions with ports, conditions, stopping work, simulated time.
