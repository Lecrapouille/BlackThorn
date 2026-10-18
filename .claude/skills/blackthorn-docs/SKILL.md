---
name: blackthorn-docs
description: Edit or extend the BlackThorn documentation (README.md, doc/ numbered chapters 01–09, doc/README.md index, src/Oakular/README.md). Use when documenting a node, a YAML field, an API change, an Oakular feature, or when asked to fix or rewrite docs.
---

# BlackThorn documentation

Audience: developers who may never have written a behavior tree, often
French. Simple English: short sentences, present tense, common words, no
idioms; define each behavior-tree word once (01-Introduction.md §2 is the
glossary). Emojis in headings, as in the other chapters.

## Map (general → specific)

| File | Holds |
|------|-------|
| README.md | pitch, YAML + C++ glance, why, quick start, doc table |
| doc/README.md | index: Learn (1–5), Reference (6–9), quick paths |
| 01-Introduction.md | glossary, statuses, one tick, comparison with BT.CPP / BrainTree |
| 02-Getting-Started.md | prerequisites, build, examples, Oakular live view, first-tree tutorial |
| 03-YAML-Format.md | sections, node syntax, YAML-readable types, parameters/ports, `_id`, SubTrees, builder errors |
| 04-Nodes.md | every node: behavior table, YAML fields, C++ constructors |
| 05-Your-Own-Nodes.md | factory, lambdas, CallbackLeaf + ports, creators, halt vs reset, clock, C++ trees |
| 06-Blackboard.md | API, strict types, nested values, `${}`, subtree scopes |
| 07-Oakular.md | modes, visualizer, protocol, embedding, BlackThorn.mk, build switches |
| 08-API-Reference.md | classes by task |
| 09-Examples.md | one section per doc/examples folder |
| src/Oakular/README.md | `oakular::Editor` API, shortcuts, subtree tools |

Put a fact in one place and link to it. README.md and doc/README.md hold the
chapter tables: keep them in sync when a chapter is added or renamed.

## Rules

- Every code excerpt must compile and every quoted output must come from a
  real run (write the program in a scratch folder, link the sources as in
  02-Getting-Started.md §4 step 3, paste the output).
- Check behavior in `src/BlackThorn/Nodes/Tree.cpp` and the YAML keys in
  `src/BlackThorn/Builder/Builder.cpp` before writing; mark what is C++ only
  or not implemented, never document what does not exist.
- Each chapter: what you will learn → tutorial(s) → reference tables → next.
- Text diagrams in ```text blocks, Mermaid for sequences; tables for references.
- Link anchors: GitHub drops emojis from heading ids (`## 🚀 2. Build` →
  `#-2-build`). Do not use emojis with U+FE0F (⚠️ ⏱️ 🖥️ 👁️ ✏️ 🎛️ …) in
  headings that are link targets: the selector stays in the id.
