# 🎯 Node Types Guide

📚 Complete guide to all node types available in BlackThorn.

---

## 🦠 Node Lifecycle

All nodes follow this lifecycle:

1. **onSetUp()** - Called on first execution (when status != RUNNING).
2. **onRunning()** - Main execution logic (called every tick while running).
3. **onTearDown(Status)** - Called when transitioning from RUNNING to SUCCESS/FAILURE.
4. **onHalt()** - Called when halt() is invoked on a RUNNING node.

For more details, see [API Reference](api-reference.md).

---

## Concrete Node Types

### 🧩 Composite Components

- ▶️ **Sequence**: Executes children in order until one fails.
- ➡️ **Selector**: Tries each child in order until one succeeds.
- ⏸️ **Parallel**: Runs all children, configurable success/failure thresholds.
- 🔀 **ParallelAll**: Runs all children, all must succeed/fail
- ⏯️ **SequenceWithMemory**: Remembers position, continues where it stopped.
- ⏮️ **ReactiveSequence**: Always restarts from the first child on every tick.
- ↪️ **SelectorWithMemory**: Remembers position, continues where it stopped.
- 🔄 **ReactiveSelector**: Always restarts from the first child on every tick.

### 🎭 Decorator Components

- 🌖 **Inverter**: Inverts SUCCESS ↔ FAILURE.
- 🟢 **ForceSuccess**: Force returning SUCCESS.
- 🔴 **ForceFailure**: Force returning FAILURE.
- 🔂 **Repeater**: Repeats the child N times or infinitely.
- 🎉 **UntilSuccess**: Repeats until the child succeeds.
- 💀 **UntilFailure**: Repeats until the child fails.
- ⏱️ **Timeout**: Limits the execution time of its child.
- ⏳ **Delay**: Waits before starting to execute its child.
- ❄️ **Cooldown**: Prevents re-execution for a period after completion.
- 1️⃣ **RunOnce**: Executes the child only once and caches the result.

### 🍁 Leaf Components

- ✅ **Success**: Always returns SUCCESS.
- ❌ **Failure**: Always returns FAILURE.
- ❓ **Condition**: Condition from a lambda returning a boolean.
- 🔨 **Action**: Abstract base for custom actions (override `onRunning()`).
- ⏳ **Wait**: Waits for a specified duration then returns SUCCESS.
- 📝 **SetBlackboard**: Writes a value to the blackboard.

---

## 🪵 SubTree Node

Executes another tree instance as a reusable subtree.

```yaml
BehaviorTree:
  SubTree:
    name: ProcessTarget
    reference: EngageEnemy
    parameters:
      target: ${primary_enemy}

SubTrees:
  EngageEnemy:
    Sequence:
      children:
        - Action:
            name: AimWeapon
            parameters:
              enemy: ${target}  # Reads from child scope
```

---

## 🍁 Leaf Nodes

### ✅ Success

Simple leaf that always returns SUCCESS.

**YAML:**

```yaml
- Success
```

### ❌ Failure

Simple leaf that always returns FAILURE.

**YAML:**

```yaml
- Failure
```

### ❓ Condition

Evaluates a condition and returns SUCCESS or FAILURE. Can never return RUNNING.

**Examples of usage:**

- Boolean checks.
- Sensor readings.
- State validation.

**YAML:**

```yaml
- Condition:
    name: "IsEnemyVisible"
    parameters:
      range: 100.0
```

The field `parameters:` is optional.

**C++:**

```cpp
auto condition = bt::Node::create<bt::Condition>(
    [bb = blackboard]() {
        return bb->get<int>("health").value_or(0) > 50;
    },
    blackboard
);
```

### 🔨 Action

Either performs a short work and and returns SUCCESS, FAILURE
or performs a chunk of a long work and return RUNNING.

**Behavior:**

- Return SUCCESS if action completes successfully.
- Return FAILURE if action fails.
- Return RUNNING if action is still executing (need more time before succeeding ot failing).

**YAML:**

```yaml
- Action:
    name: "PickUpObject"
    parameters:
      object_id: "red_cube"
      grip_force: 0.7
      max_attempts: 3
```

The field `parameters:` is optional.

**C++:**

```cpp
class MoveToTarget final: public bt::Action {
public:
    explicit MoveToTarget(bt::Blackboard::Ptr bb)
        : m_blackboard(bb), m_attempts(0)
    {}

    bt::Status onRunning() override {
        // Retrieve parameters from blackboard with default values
        auto object_id = m_blackboard->get<std::string>("object_id");
        auto grip_force = m_blackboard->get<float>("grip_force", 0.5f);
        auto max_attempts = m_blackboard->get<size_t>("max_attempts", 1);

        // Attempt to grasp the object
        if (m_attempts >= max_attempts) {
            return bt::Status::FAILURE;
        }

        if (grasp(object_id, grip_force)) {
            m_blackboard->set("grasped_object", object_id);
            return bt::Status::SUCCESS;
        }

        m_attempts++;
        return bt::Status::RUNNING;
    }

    bt::Status onSetUp() override {
        m_attempts = 0;
    }

private:
    bool grasp(std::string object_id, float grip_force);

    bt::Blackboard::Ptr m_blackboard;
    size_t m_attempts;
};
```

### ⏳ Wait

Waits for a specified duration and then returns SUCCESS. This is useful for:

- Adding pauses in behavior sequences.
- Implementing timer-based behaviors.
- Waiting for external events with a timeout.

**Behavior:**

- Returns RUNNING while the wait period is active.
- Returns SUCCESS when the wait period completes.

**YAML:**

```yaml
- Wait:
    milliseconds: 2000
```

**C++:**

```cpp
auto wait = bt::Node::create<bt::Wait>(2000);  // 2 seconds
```

### 📝 SetBlackboard

Writes a value to the blackboard and returns SUCCESS. This is useful for:

- Setting state variables during tree execution.
- Initializing values before subtrees.
- Passing data between tree branches.

**Behavior:**

- Writes the specified key-value pair to the blackboard.
- Always returns SUCCESS.

**YAML:**

```yaml
- SetBlackboard:
    key: "target_found"
    value: "true"
```

**C++:**

```cpp
auto bb = std::make_shared<bt::Blackboard>();
auto setNode = bt::Node::create<bt::SetBlackboard>("target_found", "true", bb);
```

---

## 🎭 Decorator Nodes

Since decorators wrap a single child node the YAML syntax is `child:`:

```yaml
- <DecoratorType>:
    child:
      - <ChildNode>
```

### 🌖 Inverter

Inverts SUCCESS ↔ FAILURE. RUNNING passes through.

**Behavior:**

- Return SUCCESS if child returns FAILURE.
- Return FAILURE if child returns SUCCESS.
- Return RUNNING if child returns RUNNING.

**Mnemonic:** Negate, Not.

**Example of usages:**

- Checking if something is NOT true.
- Creating "unless" conditions.

**YAML:**

```yaml
- Inverter:
    child:
      - Condition:
          name: "IsEnemyVisible"
```

### 🟢 Force Success

Returns SUCCESS if child fails, otherwise passes through status. Do not confuse it with the leaf `Success`!

**Behavior:**

- If child returns SUCCESS or RUNNING, returns the same status.
- If child returns FAILURE, returns SUCCESS instead.

**Example of usages:**

- Implementing "try but don't fail" semantics.
- Optional tasks that shouldn't cause the whole sequence to fail.
- Debugging behavior trees.
- Temporarily disabling failure conditions.
- Testing tree structure.

**YAML:**

```yaml
- ForceSuccess:
    child:
      - Action:
          name: "OptionalTask"
```

### 🔴 Force Failure

Returns FAILURE if child succeeds, otherwise passes through status. Do not confuse it with the leaf `Failure`!

**Behavior:**

- If child returns FAILURE or RUNNING, returns the same status.
- If child returns SUCCESS, returns FAILURE instead.

**Example of usages:**

- Testing negative conditions.
- Preventing specific success cases.
- Implementing "succeed only if not" semantics.

**YAML:**

```yaml
- ForceFailure:
    child:
      - Action:
          name: "UndesiredResult"
```

### 🔂 Repeater

Repeats its child node a specified number of times (0 = infinite). The child is reset and executed again after each completion, regardless of whether it returned SUCCESS or FAILURE.

**Behavior:**

- Repeats the child N times (or infinitely if `times: 0`)
- Ignores the child's SUCCESS/FAILURE status - always continues until limit reached
- Returns SUCCESS after completing all repetitions
- Returns RUNNING while executing repetitions
- Unlike BehaviorTree.CPP, this implementation does not use a `while` loop internally - the tree engine handles `tick()` calls, allowing proper visualization and reactivity between iterations.

**Example of usages:**

- Patrol N waypoints
- Fire weapon N times
- Execute animation cycle N times

```yaml
- Repeat:
    times: 3
    child:
      - Action:
          name: "TryOpenDoor"
```

### 🎉 UntilSuccess

Repeats its child until it succeeds, then returns SUCCESS. Can optionally limit the number of attempts (0 = infinite). Unlike BehaviorTree.CPP, this implementation does not use a `while` loop internally - the tree engine handles `tick()` calls, allowing proper visualization and reactivity between iterations.

**Behavior:**

- Repeats the child until it returns SUCCESS (or until attempts limit if set)
- Returns SUCCESS as soon as the child succeeds
- Returns FAILURE if all attempts are exhausted (when `attempts:` > 0)
- Returns RUNNING while the child is executing
- If `attempts: 0` (default), repeats indefinitely until success
- If `attempts: N`, retries up to N times, then returns FAILURE if still failing

**Example of usages:**

- Wait for a connection (indefinitely or with timeout)
- Poll until a condition becomes true
- Retry an operation until it succeeds (with optional limit)

**YAML:**

```yaml
# Infinite attempts (default)
- UntilSuccess:
    child:
      - Action:
          name: "WaitForConnection"

# Limited attempts
- UntilSuccess:
    attempts: 5
    child:
      - Action:
          name: "Connect"
```

### 💀 UntilFailure

Repeats its child until it fails, then returns SUCCESS. This is useful for monitoring or polling scenarios where you want to detect when something stops working. Can optionally limit the number of attempts (0 = infinite). Unlike BehaviorTree.CPP, this implementation does not use a `while` loop internally - the tree engine handles `tick()` calls, allowing proper visualization and reactivity between iterations.

**Behavior:**

- Repeats the child until it returns FAILURE (or until attempts limit if set)
- Returns SUCCESS as soon as the child fails (counter-intuitive but useful for monitoring)
- Returns FAILURE if all attempts are exhausted (when `attempts:` > 0 and child keeps succeeding)
- Returns RUNNING while the child is executing
- If `attempts: 0` (default), repeats indefinitely until failure
- If `attempts: N`, retries up to N times, then returns FAILURE if child keeps succeeding

**Example of usages:**

- Monitor battery level until it's low (then succeed to trigger recharge)
- Poll a sensor until it detects an anomaly
- Wait until a condition becomes false (with optional timeout)

**YAML:**

```yaml
# Infinite attempts (default)
- UntilFailure:
    child:
      - Condition:
          name: "IsBatteryOK"

# Limited attempts
- UntilFailure:
    attempts: 10
    child:
      - Condition:
          name: "IsSensorOK"
```

### ⏱️ Timeout

Limits the execution time of its child. Useful for:

- Preventing actions from running too long.
- Time-critical operations.
- Implementing fallback triggers.

**Behavior:**

- Starts a timer when the child begins execution.
- Returns FAILURE if the timeout is reached and halts the child.
- Otherwise, returns the child's status.

**Example YAML Syntax:**

```yaml
- Timeout:
    milliseconds: 5000
    child:
      - Action:
          name: "LongRunningTask"
```

### ⏳ Delay

Waits for a specified duration before starting to execute its child. Useful for:

- Adding pauses between actions.
- Implementing cooldown periods.
- Delaying the start of animations or actions.

**Behavior:**

- Returns RUNNING during the delay period.
- Once the delay has passed, ticks the child and returns its status.

**Example YAML Syntax:**

```yaml
- Delay:
    milliseconds: 2000
    child:
      - Action:
          name: "AttackAfterDelay"
```

### ❄️ Cooldown

Prevents the child from being executed more frequently than a specified interval. Useful for:

- Rate-limiting actions (like attacks or abilities).
- Preventing spam behavior.
- Resource management.

**Behavior:**

- After the child completes (SUCCESS or FAILURE), starts a cooldown period.
- Returns FAILURE during cooldown if the node is ticked again.
- Once cooldown passes, allows the child to execute again.

**Example YAML Syntax:**

```yaml
- Cooldown:
    milliseconds: 3000
    child:
      - Action:
          name: "FireWeapon"
```

### 1️⃣ RunOnce

Executes its child only once and remembers the result. Useful for:

- One-time initialization tasks.
- Actions that should only happen once per tree lifetime.
- Tutorial triggers.

**Behavior:**

- First execution: runs the child and caches its result.
- Subsequent executions: immediately returns the cached result.
- Can be reset using `reset()` to allow re-execution.

**Example YAML Syntax:**

```yaml
- RunOnce:
    child:
      - Action:
          name: "PlayIntroAnimation"
```

---

## 🧩 Composite Nodes

Composites have multiple children. YAML syntax:

```yaml
- <CompositeType>:
    children:
      - <Child1>
      - <Child2>
      - <Child3>
```

### ▶️ Sequence

Execute children in order until one fails. Returns SUCCESS only if all succeed. It acts like the `and` operator.

**Behavior:**

- Tick each child node in order.
- Return SUCCESS if all children succeed.
- Return FAILURE if any child fails.
- Return RUNNING if any child is running.

**Mnemonic:** "And", "Do In Order".

```yaml
- Sequence:
    children:
      - Action:
          name: "OpenDoor"
      - Action:
          name: "Walk"
      - Action:
          name: "CloseDoor"
```

Used for tasks that must be completed in sequence, like:

- Open the door.
- Walk into the room.
- Close the door.

Beware! If the action Walk fails, the door will remain open since the last action CloseDoor is skipped.

### ➡️ Selector

Tries children in order until one succeeds. Returns FAILURE only if all fail. It acts like the `or` operator. This is useful for fallback strategies.

**Behavior:**

- Ticks each child node in order.
- Returns SUCCESS if any child succeeds.
- Returns FAILURE if all children fail.
- Returns RUNNING if any child is running.

**Mnemonic:** "Or", "Fallback", "Try Until Success", "Try alternatives".

**YAML:**

```yaml
- Selector:
    children:
      - Action:
          name: "FindEnemy"
      - Action:
          name: "PatrolArea"
      - Action:
          name: "ReturnToBase"
```

Explanation:

- Try to find an enemy.
- If no enemy found, patrol the area.
- If patrol fails, return to base.

### ⏸️ Parallel Sequences

Executes all children simultaneously. Configurable success/failure thresholds.

**Behavior:**

- Ticks all `N` children concurrently.
- Returns SUCCESS if at least M children succeed (`M` is configurable, default is `success_threshold: 1`).
- Returns FAILURE if more than `N-M` children fail.
- Returns RUNNING otherwise.

**Example of usages:**

- You need any one of several conditions to trigger a behavior.
- You want redundant systems where only some need to succeed.
- You need to monitor multiple conditions with different priorities.

```yaml
- Parallel:
    success_threshold: 2  # Require at least 2 successes
    failure_threshold: 2  # Require at least 2 failures
    children:
      - Action: { name: "MonitorBattery" }
      - Action: { name: "MonitorObstacles" }
      - Action: { name: "MonitorEnemies" }
```

**C++:**

```cpp
auto parallel = bt::Node::create<bt::Parallel>(2, 2);
```

### 🔀 ParallelAll Sequences

Executes all children simultaneously. Requires all to succeed or fail based on policies.

**Behavior:**

- Ticks all children simultaneously.
- Returns SUCCESS only if ALL children succeed.
- Returns FAILURE if ANY child fails.
- Returns RUNNING otherwise.

**Example of usages:**

- All tasks must succeed simultaneously.
- Background processes need to run alongside main tasks.
- Multiple conditions must all be satisfied.

**YAML:**

```yaml
- ParallelAll:
    children:
      - Action:
          name: "MoveForward"
      - Action:
          name: "ScanEnvironment"
      - Action:
          name: "MaintainBalance"
```

**C++:**

```cpp
auto parallelAll = bt::Node::create<bt::ParallelAll>();
```

### ⏯️ Sequence With Memory

Same than Sequence but remembers position. Continues from where it stopped on previous tick.

**Behavior:**

- Similar to Sequence, but it remembers which child it was executing if a child returns RUNNING.
- On the next tick, it will continue from the last running child instead of starting from the beginning.
- Returns SUCCESS if all children succeed.
- Returns FAILURE if any child fails.
- Returns RUNNING if the current child is running.

**Mnemonic:** "Continue In Order", "Stateful Sequence".

**Example of Usages:**

- Tasks that continue over multiple ticks.
- Preserving progress in a sequence.
- Avoiding redundant execution of already completed steps.

**YAML:**

```yaml
- SequenceWithMemory:
    children:
      - Action:
          name: "PrepareTask"
      - Action:
          name: "LongRunningTask"
      - Action:
          name: "CompleteTask"
```

### ⏮️ Reactive Sequence

Same than Sequence but always restarts from first child each tick. Re-checks all previous children.

**Behavior:**

- Always starts from the first child.
- Re-evaluates all previously successful children at each tick.
- If a previously successful child now fails, the sequence fails.
- Returns SUCCESS if all children succeed.
- Returns FAILURE if any child fails.
- Returns RUNNING if the current child is running.

**Mnemonic:** "Check all"

**Example of usages:**

- Conditions that must remain true throughout an action.
- Safety checks that need constant verification (battery voltage).
- Tasks that should stop immediately if prerequisites are no longer met.

**YAML:**

```yaml
- ReactiveSequence:
    children:
      - Condition:
          name: "IsPathClear"
      - Action:
          name: "MoveForward"
```

### ↪️ Selector With Memory

Similar to Fallback but remembers position. Continues from where it stopped on previous tick (tries until success).

**Behavior:**

- Similar to Selector, but it remembers which child it was executing if a child returns RUNNING.
- On the next tick, it will continue from the last running child instead of starting from the beginning.
- Returns SUCCESS if any child succeeds.
- Returns FAILURE if all children fail.
- Returns RUNNING if the current child is running.

**Mnemonic:** Remember And Try

**Examples of Usage:**

- Complex fallback scenarios that span multiple ticks.
- Recovery strategies that need to continue from where they left off.
- Avoiding restarting already attempted recovery steps.

**YAML:**

```yaml
- SelectorWithMemory:
    children:
      - Action:
          name: "TryPrimaryMethod"
      - Action:
          name: "TryBackupMethod"
      - Action:
          name: "EmergencyProtocol"
```

### 🔄 Reactive Selector

Similar to Fallback but it always restarts from first child each tick. Re-checks all previous children.

**Behavior:**

- Always starts from the first child.
- Re-evaluates previously failed children at each tick.
- If a previously failed child now succeeds, the selector succeeds.
- Returns SUCCESS if any child succeeds.
- Returns FAILURE if all children fail.
- Returns RUNNING if the current child is running.

**Mnemonic:** Check Until Success

**Examples of usage:**

- Continuously checking if preferred options become available.
- Reactive planning that adapts to changing conditions.
- Implementing priority-based decisions that need constant reassessment.

**YAML:**

```yaml
- ReactiveSelector:
    children:
      - Condition:
          name: "IsPathA"
      - Condition:
          name: "IsPathB"
      - Action:
          name: "FindAlternativePath"
```
