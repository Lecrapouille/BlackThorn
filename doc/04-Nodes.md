# 🎯 4. The nodes

In this page you meet every built-in node: what it does, what it returns, its YAML fields, and when to use it. Start with the table, then read the section of the node you need.

---

## 🧭 1. Which node do I need?

| I want to… | Node |
|------------|------|
| do steps in order, stop at the first failure | ▶️ [`Sequence`](#sequence) |
| try options in order, stop at the first success | ➡️ [`Selector`](#selector) |
| re-check a condition at every tick while an action runs | ⏮️ [`ReactiveSequence`](#reactivesequence-and-reactiveselector) |
| run several children in the same tick | ⏸️ [`Parallel`](#parallel) |
| turn "no" into "yes" | 🌖 [`Inverter`](#inverter) |
| try a step that may fail without failing the whole tree | 🟢 [`ForceSuccess`](#forcesuccess-and-forcefailure) |
| repeat N times | 🔂 [`Repeat`](#repeat) |
| retry a failed step without redoing the steps before it | ⏯️ [`SequenceWithMemory`](#sequencewithmemory-and-selectorwithmemory) |
| retry until it works | 🎉 [`UntilSuccess`](#untilsuccess-and-untilfailure) |
| give up after a time | ⏱️ [`Timeout`](#timeout) |
| wait before starting | ⏳ [`Delay`](#delay), [`Wait`](#wait) |
| limit how often something runs | ❄️ [`Cooldown`](#cooldown) |
| run something only once | 1️⃣ [`RunOnce`](#runonce) |
| call my C++ code | 🔨 [`Action`](#action), ❓ [`Condition`](#condition) |
| write a value | 📝 [`SetBlackboard`](#setblackboard) |
| reuse a tree | 🪵 [`SubTree`](03-YAML-Format.md#-5-subtrees) |

---

## 🧩 2. Composites: several children

A composite ticks its children one after the other, in the order of the file, and decides when to stop.

```yaml
- <Type>:
    children:
      - <child 1>
      - <child 2>
```

### Sequence

▶️ Ticks its children in order. **Fails** as soon as one child fails. **Succeeds** when all children succeed. Think "and", "do in order".

| Child returns | The sequence… |
|---------------|---------------|
| `SUCCESS` | goes on to the next child, in the same tick |
| `FAILURE` | stops and returns `FAILURE` |
| `RUNNING` | stops and returns `RUNNING`; at the next tick it **resumes at this child** |

```yaml
- Sequence:
    children:
      - Action: { name: OpenDoor }
      - Action: { name: Walk }
      - Action: { name: CloseDoor }
```

> ⚠️ If `Walk` fails, `CloseDoor` never runs: the door stays open. Put the clean-up in a `Selector` branch, or in a `ForceSuccess`, if it must happen.

Because the sequence resumes at the running child, the children **before** it are not ticked again. A condition in front of a long action is checked only once. To re-check it at every tick, use a `ReactiveSequence`, or a `Timeout` around the action ([chapter 5 §6](05-Your-Own-Nodes.md#tutorial-the-cleaning-robot) shows both).

### Selector

➡️ Ticks its children in order. **Succeeds** as soon as one child succeeds. **Fails** when all children fail. Think "or", "fallback", "try the next idea".

| Child returns | The selector… |
|---------------|---------------|
| `FAILURE` | goes on to the next child, in the same tick |
| `SUCCESS` | stops and returns `SUCCESS` |
| `RUNNING` | stops and returns `RUNNING`; at the next tick it resumes at this child |

```yaml
- Selector:
    children:
      - Action: { name: FindEnemy }
      - Action: { name: PatrolArea }
      - Action: { name: ReturnToBase }
```

*Find an enemy; if none, patrol; if the patrol fails, go back to base.*

### ReactiveSequence and ReactiveSelector

⏮️ 🔄 Same rules as `Sequence` and `Selector`, but they **restart from the first child at every tick**. The children before the running one are checked again each time.

```text
ReactiveSequence
├── Condition  BatteryOk     ← ticked at every tick
└── Action     Clean         ← stops as soon as BatteryOk fails
```

Use them for safety checks that must stay true during a long action.

When an earlier child decides (the condition fails in a `ReactiveSequence`, or succeeds in a `ReactiveSelector`), the child that was `RUNNING` is **halted**: its reset function is called, so a motor stops ([chapter 5 §5](05-Your-Own-Nodes.md#-5-stop-the-work-reset-and-halt)).

```yaml
- ReactiveSequence:
    children:
      - Condition: { name: BatteryOk }
      - Action: { name: Clean }
```

### SequenceWithMemory and SelectorWithMemory

⏯️ ↪️ Same rules as `Sequence` and `Selector`, but they **remember their position after they end**:

| Node | After it ends with… | The next run starts at… |
|------|---------------------|-------------------------|
| `SequenceWithMemory` | `FAILURE` (child *k* failed) | child *k*: the steps that already succeeded are not done again |
| `SequenceWithMemory` | `SUCCESS` | the first child |
| `SelectorWithMemory` | `SUCCESS` (child *k* succeeded) | child *k*: the options that already failed are not tried again |
| `SelectorWithMemory` | `FAILURE` | the first child |

`tree->reset()` sends them back to the first child. A plain `Sequence` starts again from the first child after a failure.

```yaml
- SequenceWithMemory:        # pick, carry, drop: a failed drop does not pick again
    children:
      - Action: { name: Pick }
      - Action: { name: Carry }
      - Action: { name: Drop }
```

### Parallel

⏸️ Ticks **all** its children at each tick (one after the other, in the same thread), then counts.

Two ways to write it, and you must choose one:

| Fields | Returns `SUCCESS` when… | Returns `FAILURE` when… |
|--------|-------------------------|-------------------------|
| `success_threshold: M`, `failure_threshold: F` | at least `M` children succeed | at least `F` children fail |
| `success_on_all: true/false`, `fail_on_all: true/false` | all children succeed (`true`) or one does (`false`) | all children fail (`true`) or one does (`false`) |

Otherwise it returns `RUNNING`. Success is tested first. A `Parallel` with none of these fields is refused (`Missing policies or thresholds`).

```yaml
- Parallel:
    success_threshold: 2     # two monitors must agree
    failure_threshold: 2
    children:
      - Action: { name: MonitorBattery }
      - Action: { name: MonitorObstacles }
      - Action: { name: MonitorEnemies }

- Parallel:                  # walk while scanning; one failure stops both
    success_on_all: true
    fail_on_all: false
    children:
      - Action: { name: MoveForward }
      - Action: { name: ScanEnvironment }
```

In C++: `bt::Parallel(min_success, min_fail)` and `bt::ParallelAll(success_on_all, fail_on_all)`.

---

## 🎭 3. Decorators: one child

A decorator wraps **exactly one** child, written as a one-element list:

```yaml
- <Type>:
    child:
      - <the child>
```

### Inverter

🌖 Swaps `SUCCESS` and `FAILURE`. `RUNNING` stays `RUNNING`. Think "not", "unless".

```yaml
- Inverter:
    child:
      - Condition: { name: IsEnemyVisible }   # succeeds when NO enemy is visible
```

### ForceSuccess and ForceFailure

🟢 🔴 `ForceSuccess` returns `SUCCESS` when its child ends, whatever the child returned; `ForceFailure` returns `FAILURE`. `RUNNING` stays `RUNNING`. Do not mix them up with the leaves `Success` and `Failure`, which have no child.

```yaml
- ForceSuccess:              # an optional step: the sequence goes on anyway
    child:
      - Action: { name: TakePhoto }
```

### Repeat

🔂 Runs its child again each time it ends, whatever it returned, `times` times. Returns `RUNNING` between two runs, `SUCCESS` after the last one. `times: 0` (the default) repeats forever.

```yaml
- Repeat:                    # "Repeater" is accepted too
    times: 3
    child:
      - Action: { name: FireWeapon }
```

The number can come from the blackboard, with a port named `repetitions`:

```yaml
- Repeat:
    parameters:
      repetitions: ${shots}
    child:
      - Action: { name: FireWeapon }
```

Each run of the child starts in a new tick: the tree stays reactive and Oakular shows every run (BehaviorTree.CPP loops inside one tick instead).

### UntilSuccess and UntilFailure

🎉 `UntilSuccess` runs its child again until it succeeds, then returns `SUCCESS`. 💀 `UntilFailure` runs its child again until it fails, then returns **`SUCCESS`** (the awaited event happened).

| Field | Default | Meaning |
|-------|---------|---------|
| `attempts` | `0` | `0`: try forever. `N`: after `N` runs that did not give the awaited status, return `FAILURE`. |

```yaml
- UntilSuccess:              # try to connect 5 times, then give up
    attempts: 5
    child:
      - Action: { name: Connect }

- UntilFailure:              # watch the battery until it is low
    child:
      - Condition: { name: IsBatteryOk }
```

### Timeout

⏱️ Gives its child a time limit. When the time is over, it **halts** the child and returns `FAILURE`. Before, it returns the status of its child.

| Field | Default | Meaning |
|-------|---------|---------|
| `milliseconds` | `1000` | the limit, counted from the first tick |

```yaml
- Timeout:
    milliseconds: 5000
    child:
      - Action: { name: LongRunningTask }
```

The limit can come from the blackboard with a port named `milliseconds` (`parameters: { milliseconds: ${limit} }`).

### Delay

⏳ Returns `RUNNING` during `milliseconds` (default `1000`), then ticks its child and returns the status of the child.

```yaml
- Delay:
    milliseconds: 2000
    child:
      - Action: { name: AttackAfterDelay }
```

### Cooldown

❄️ After its child ends, refuses to run it again during `milliseconds` (default `1000`): a tick in this period returns `FAILURE` at once.

```yaml
- Cooldown:
    milliseconds: 3000
    child:
      - Action: { name: FireWeapon }
```

### RunOnce

1️⃣ Runs its child until it ends, then keeps the result: the next ticks return this result without ticking the child. `reset()` of the tree forgets it.

```yaml
- RunOnce:
    child:
      - Action: { name: PlayIntroAnimation }
```

> ⏰ **Time.** `Timeout`, `Delay`, `Cooldown` and `Wait` read the clock of the tree: the wall clock by default, or the clock you give with `tree->clock(...)`. A simulator gives its simulated time, so that a test runs the same way, fast or slow ([chapter 5 §6](05-Your-Own-Nodes.md#-6-time-a-simulated-clock)).

---

## 🍁 4. Leaves: no child

### Action

🔨 Calls your C++ code. `name` is the name registered in the `NodeFactory`. Your code returns `SUCCESS`, `FAILURE` or `RUNNING`.

```yaml
- Action:
    name: PickUpObject
    parameters:              # optional: the ports of the node
      object: ${target}
      grip_force: 0.7
```

How to write the C++ side: [chapter 5](05-Your-Own-Nodes.md).

### Condition

❓ Calls your C++ function that returns `true` (`SUCCESS`) or `false` (`FAILURE`). It never returns `RUNNING`: a condition answers at once.

```yaml
- Condition:
    name: IsEnemyVisible
```

### Success and Failure

✅ ❌ Always return `SUCCESS` (or `FAILURE`). Useful to test a tree, or as a placeholder while you write it.

```yaml
- Success:
- Failure:
```

### Wait

⏳ Returns `RUNNING` during `milliseconds` (default `1000`), then `SUCCESS`.

```yaml
- Wait:
    milliseconds: 2000
```

### SetBlackboard

📝 Writes `value` under `key` in the blackboard and returns `SUCCESS`. The value is stored as a **string**: read it with `get<std::string>`.

```yaml
- SetBlackboard:
    key: target_found
    value: "true"
```

---

## 🧰 5. The same nodes in C++

Every node is a class of the `bt` namespace. You create it inside a tree with `createRoot<T>(...)`, `addChild<T>(...)` (composites) or `createChild<T>(...)` (decorators); the arguments go to its constructor.

| Class | Constructor |
|-------|-------------|
| `Sequence`, `ReactiveSequence`, `SequenceWithMemory` | `()` |
| `Selector`, `ReactiveSelector`, `SelectorWithMemory` | `()` |
| `Parallel` | `(int min_success, int min_fail)` |
| `ParallelAll` | `(bool success_on_all = true, bool fail_on_all = true)` |
| `Inverter`, `ForceSuccess`, `ForceFailure`, `RunOnce` | `()` |
| `Repeater` | `(size_t repetitions = 0)` |
| `UntilSuccess`, `UntilFailure` | `(size_t attempts = 0)` |
| `Timeout`, `Delay`, `Cooldown` | `(size_t milliseconds = 1000)` |
| `Wait` | `(size_t milliseconds = 1000)` |
| `SetBlackboard` | `(std::string key, std::string value)` |
| `Success`, `Failure` | `()` |
| `Condition` | `(std::function<bool()>)` |
| `CallbackLeaf` (the `Action`) | `(std::function<Status()>)`, plus an optional blackboard or reset function |

An example is in [chapter 5 §7](05-Your-Own-Nodes.md#-7-build-a-tree-in-c-without-yaml).

Next: [05-Your-Own-Nodes.md](05-Your-Own-Nodes.md).
