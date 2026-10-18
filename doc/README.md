# 📚 BlackThorn documentation

Read the chapters in order: each one uses the words of the previous ones. Chapters 1 to 5 are for everybody, even if you never wrote a behavior tree. Chapters 6 to 9 are references: open them when you need them.

## 🎓 Learn

| # | Chapter | You will learn | Time |
|---|---------|----------------|------|
| 1 | [Introduction](01-Introduction.md) | What a behavior tree is: tick, status, node families, blackboard | 10 min |
| 2 | [Getting started](02-Getting-Started.md) | Build BlackThorn, run the examples, write and run your first tree | 20 min |
| 3 | [The YAML format](03-YAML-Format.md) | Describe a tree, its blackboard and its subtrees in a file | 15 min |
| 4 | [The nodes](04-Nodes.md) | Every built-in node, what it returns and when to use it | 30 min |
| 5 | [Your own nodes](05-Your-Own-Nodes.md) | Write actions and conditions in C++, read ports, stop work, use a simulated clock | 30 min |

## 📖 Reference

| # | Chapter | Contents |
|---|---------|----------|
| 6 | [The blackboard](06-Blackboard.md) | Types, nested values, `${key}`, scopes of subtrees, ports |
| 7 | [Oakular](07-Oakular.md) | The editor and the live visualizer, its protocol, embedding it in your application |
| 8 | [API reference](08-API-Reference.md) | The classes, grouped by task |
| 9 | [Examples](09-Examples.md) | Every example of `doc/examples/` and what it shows |

The editor API (`oakular::Editor`) has its own page: [src/Oakular/README.md](../src/Oakular/README.md).

## 🧭 Quick paths

- *"What is a behavior tree?"* → [1. Introduction](01-Introduction.md)
- *"I want to see a tree run."* → [2. Getting started](02-Getting-Started.md)
- *"Which node do I need?"* → [4. The nodes](04-Nodes.md), table at the top
- *"My action must talk to my robot or my game."* → [5. Your own nodes](05-Your-Own-Nodes.md)
- *"I want to draw trees and watch them run."* → [7. Oakular](07-Oakular.md)
- *"I want to change BlackThorn itself."* → [AGENTS.md](../AGENTS.md) for the conventions

## 🌍 A note on the language

This documentation is written in simple English, with short sentences and common words, so that readers whose first language is not English (French readers in particular) can follow it easily.
