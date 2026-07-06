#!/usr/bin/env python3
"""Generate benchmark tree files for BlackThorn (YAML) and BehaviorTree.CPP (XML)."""

from __future__ import annotations

import argparse
from pathlib import Path


def indent_lines(lines: list[str], spaces: int) -> list[str]:
    pad = " " * spaces
    return [pad + line if line else line for line in lines]


def yaml_success_leaf(name: str) -> list[str]:
    return [
        "- Success:",
        f"    name: {name}",
    ]


def yaml_setblackboard(key: str, value: str) -> list[str]:
    return [
        "- SetBlackboard:",
        f"    key: {key}",
        f"    value: \"{value}\"",
    ]


def yaml_read_ports_action(index: int) -> list[str]:
    return [
        "- Action:",
        "    name: ReadPorts",
        "    parameters:",
        f"      a: ${{a_{index}}}",
        f"      b: ${{b_{index}}}",
        f"      label: ${{label_{index}}}",
    ]


def yaml_blackboard_port_reads(count: int = 30) -> str:
    lines = [
        "# Benchmark: resolve remapped ports from blackboard on each tick.",
        "Blackboard:",
    ]
    for i in range(count):
        lines.append(f"  a_{i}: {i + 1}")
        lines.append(f"  b_{i}: {(i + 1) * 2}")
        lines.append(f'  label_{i}: "entry-{i}"')
    lines.extend(
        [
            "",
            "BehaviorTree:",
            "  Sequence:",
            "    name: PortReads",
            "    children:",
        ]
    )
    for i in range(count):
        lines.extend(indent_lines(yaml_read_ports_action(i), 6))
    return "\n".join(lines) + "\n"


def yaml_blackboard_writes(count: int = 40) -> str:
    lines = [
        "# Benchmark: sequential SetBlackboard writes during tick.",
        "BehaviorTree:",
        "  Sequence:",
        "    name: WriteChain",
        "    children:",
    ]
    for i in range(count):
        lines.extend(indent_lines(yaml_setblackboard(f"step_{i}", str(i)), 6))
    lines.extend(indent_lines(yaml_success_leaf("Done"), 6))
    return "\n".join(lines) + "\n"


def yaml_blackboard_subtree_remap(count: int = 10) -> str:
    lines = [
        "# Benchmark: SubTree port remapping with child blackboard scopes.",
        "Blackboard:",
    ]
    for i in range(count):
        lines.append(f"  payload_{i}: {i + 1}")
        lines.append(f'  tag_{i}: "tag-{i}"')
    lines.extend(
        [
            "",
            "BehaviorTree:",
            "  Sequence:",
            "    name: RemapChain",
            "    children:",
        ]
    )
    for i in range(count):
        lines.extend(
            indent_lines(
                [
                    "- SubTree:",
                    f"    name: CopyPayload{i}",
                    "    reference: CopyPayload",
                    "    parameters:",
                    f"      a: ${{payload_{i}}}",
                    f"      b: ${{payload_{i}}}",
                    f"      label: ${{tag_{i}}}",
                ],
                6,
            )
        )
    lines.extend(indent_lines(yaml_success_leaf("Done"), 6))
    lines.extend(
        [
            "",
            "SubTrees:",
            "  CopyPayload:",
            "    Sequence:",
            "      name: CopyPayload",
            "      children:",
            "        - Action:",
            "            name: ReadPorts",
            "            parameters:",
            "              a: ${a}",
            "              b: ${b}",
            "              label: ${label}",
            "        - Success:",
            "            name: Copied",
        ]
    )
    return "\n".join(lines) + "\n"


def yaml_blackboard_large(key_count: int = 60, read_count: int = 20) -> str:
    lines = [
        "# Benchmark: large blackboard payload + port resolution.",
        "Blackboard:",
    ]
    for i in range(key_count):
        lines.extend(
            [
                f"  entity_{i}:",
                f"    id: {i}",
                f"    health: {100 - i}",
                "    position:",
                f"      x: {i * 0.5}",
                f"      y: {i * 0.25}",
                f"      z: {i * 0.1}",
            ]
        )
    lines.extend(
        [
            "",
            "BehaviorTree:",
            "  Sequence:",
            "    name: LargeBoardReads",
            "    children:",
        ]
    )
    for i in range(read_count):
        lines.extend(indent_lines(yaml_read_ports_action(i), 6))
    return "\n".join(lines) + "\n"


def xml_setblackboard(key: str, value: str, indent: int) -> list[str]:
    return indent_lines(
        [f'<SetBlackboard output_key="{key}" value="{value}"/>'], indent
    )


def xml_read_ports(index: int, indent: int) -> list[str]:
    return indent_lines(
        [
            f'<ReadPorts name="ReadPorts{index}" a="{{a_{index}}}" '
            f'b="{{b_{index}}}" label="{{label_{index}}}"/>'
        ],
        indent,
    )


def xml_script_init_port_reads(count: int) -> str:
    assignments = []
    for i in range(count):
        assignments.append(f"a_{i}:= {i + 1}")
        assignments.append(f"b_{i}:= {(i + 1) * 2}")
        assignments.append(f"label_{i}:= 'entry-{i}'")
    return "; ".join(assignments)


def xml_blackboard_port_reads(count: int = 30) -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.append("    <Sequence name=\"PortReads\">")
    lines.append(
        f'      <Script code="{xml_script_init_port_reads(count)}"/>'
    )
    for i in range(count):
        lines.extend(xml_read_ports(i, 6))
    lines.append("    </Sequence>")
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_blackboard_writes(count: int = 40) -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.append("    <Sequence name=\"WriteChain\">")
    for i in range(count):
        lines.extend(xml_setblackboard(f"step_{i}", str(i), 6))
    lines.extend(xml_always_success("Done", 6))
    lines.append("    </Sequence>")
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_blackboard_subtree_remap(count: int = 10) -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.append("    <Sequence name=\"RemapChain\">")
    init_parts = []
    for i in range(count):
        init_parts.append(f"payload_{i}:= {i + 1}")
        init_parts.append(f"tag_{i}:= 'tag-{i}'")
    lines.append(f'      <Script code="{"; ".join(init_parts)}"/>')
    for i in range(count):
        lines.append(
            f'      <SubTree ID="CopyPayload" name="CopyPayload{i}" '
            f'a="{{payload_{i}}}" b="{{payload_{i}}}" label="{{tag_{i}}}"/>'
        )
    lines.extend(xml_always_success("Done", 6))
    lines.append("    </Sequence>")
    lines.append("  </BehaviorTree>")
    lines.append("  <BehaviorTree ID=\"CopyPayload\">")
    lines.append("    <Sequence name=\"CopyPayload\">")
    lines.append(
        '      <ReadPorts name="ReadPayload" a="{a}" b="{b}" label="{label}"/>'
    )
    lines.extend(xml_always_success("Copied", 6))
    lines.append("    </Sequence>")
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_script_init_large(key_count: int) -> str:
    assignments = []
    for i in range(key_count):
        assignments.append(f"a_{i}:= {i + 1}")
        assignments.append(f"b_{i}:= {(i + 1) * 2}")
        assignments.append(f"label_{i}:= 'entry-{i}'")
    return "; ".join(assignments)


def xml_blackboard_large(key_count: int = 60, read_count: int = 20) -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.append("    <Sequence name=\"LargeBoardReads\">")
    lines.append(
        f'      <Script code="{xml_script_init_large(key_count)}"/>'
    )
    for i in range(read_count):
        lines.extend(xml_read_ports(i, 6))
    lines.append("    </Sequence>")
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def yaml_parallel_100_success() -> str:
    lines = [
        "# Benchmark: 100 Success leaves under Parallel (framework stress).",
        "BehaviorTree:",
        "  Parallel:",
        "    name: Parallel100",
        "    success_threshold: 1",
        "    failure_threshold: 101",
        "    children:",
    ]
    for i in range(100):
        lines.extend(indent_lines(yaml_success_leaf(f"Success{i}"), 6))
    return "\n".join(lines) + "\n"


def yaml_sequence_nested(depth: int, level: int = 0) -> list[str]:
    if depth <= 0:
        return indent_lines(yaml_success_leaf("Leaf"), 2)

    lines = [
        "- Sequence:",
        f"    name: Level{level}",
        "    children:",
    ]
    child = yaml_sequence_nested(depth - 1, level + 1)
    lines.extend(indent_lines(child, 6))
    return lines


def yaml_sequence_depth_50() -> str:
    lines = [
        "# Benchmark: Sequence nested 50 levels deep (indirection stress).",
        "BehaviorTree:",
        "  Sequence:",
        "    name: Root",
        "    children:",
    ]
    lines.extend(indent_lines(yaml_sequence_nested(49, 1), 4))
    return "\n".join(lines) + "\n"


def xml_root_open(main_tree: str = "MainTree") -> list[str]:
    return [
        '<?xml version="1.0"?>',
        f'<root BTCPP_format="4" main_tree_to_execute="{main_tree}">',
    ]


def xml_root_close() -> list[str]:
    return ["</root>", ""]


def xml_always_success(name: str, indent: int) -> list[str]:
    return indent_lines([f'<AlwaysSuccess name="{name}"/>'], indent)


def xml_parallel_100_success() -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.append('    <Parallel name="Parallel100" success_count="1" failure_count="100">')
    for i in range(100):
        lines.extend(xml_always_success(f"Success{i}", 6))
    lines.append("    </Parallel>")
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_sequence_nested(depth: int, level: int = 0) -> list[str]:
    if depth <= 0:
        return xml_always_success("Leaf", 0)

    name = f"Level{level}" if level > 0 else "Root"
    lines = [f'<Sequence name="{name}">']
    child = xml_sequence_nested(depth - 1, level + 1)
    lines.extend(indent_lines(child, 2))
    lines.append("</Sequence>")
    return lines


def xml_sequence_depth_50() -> str:
    lines = xml_root_open()
    lines.append("  <BehaviorTree ID=\"MainTree\">")
    lines.extend(indent_lines(xml_sequence_nested(49, 0), 4))
    lines.append("  </BehaviorTree>")
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_patrol() -> str:
    lines = xml_root_open()
    lines.extend(
        [
            "  <BehaviorTree ID=\"MainTree\">",
            '    <Sequence name="PatrolAndEngage">',
            '      <SubTree ID="PatrolRoute" name="PatrolRoute"/>',
            '      <SubTree ID="EngageEnemy" name="EngageEnemy"/>',
            '      <Action ID="ExtractTeam" name="ExtractTeam"/>',
            "    </Sequence>",
            "  </BehaviorTree>",
            "  <BehaviorTree ID=\"PatrolRoute\">",
            '    <Sequence name="PatrolRoute">',
            '      <Action ID="LoadRoute" name="LoadRoute"/>',
            '      <Action ID="FollowWaypoints" name="FollowWaypoints"/>',
            "    </Sequence>",
            "  </BehaviorTree>",
            "  <BehaviorTree ID=\"EngageEnemy\">",
            '    <Fallback name="EngageEnemy">',
            '      <Action ID="AttemptNonLethal" name="AttemptNonLethal"/>',
            '      <Action ID="NeutralizeThreat" name="NeutralizeThreat"/>',
            "    </Fallback>",
            "  </BehaviorTree>",
        ]
    )
    lines.extend(xml_root_close())
    return "\n".join(lines)


def xml_game_state() -> str:
    lines = xml_root_open()
    lines.extend(
        [
            "  <BehaviorTree ID=\"MainTree\">",
            '    <Sequence name="MissionRoot">',
            '      <SubTree ID="EvaluateEnemies" name="EvaluateEnemies"/>',
            '      <AlwaysSuccess name="MissionComplete"/>',
            "    </Sequence>",
            "  </BehaviorTree>",
            "  <BehaviorTree ID=\"EvaluateEnemies\">",
            '    <Sequence name="EvaluateEnemies">',
            '      <Action ID="LoadGameState" name="LoadGameState"/>',
            '      <Action ID="ChoosePrimaryEnemy" name="ChoosePrimaryEnemy"/>',
            "    </Sequence>",
            "  </BehaviorTree>",
        ]
    )
    lines.extend(xml_root_close())
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--yaml-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "yaml",
    )
    parser.add_argument(
        "--xml-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "xml",
    )
    args = parser.parse_args()

    args.yaml_dir.mkdir(parents=True, exist_ok=True)
    args.xml_dir.mkdir(parents=True, exist_ok=True)

    (args.yaml_dir / "parallel_100_success.yaml").write_text(
        yaml_parallel_100_success(), encoding="utf-8"
    )
    (args.yaml_dir / "sequence_depth_50.yaml").write_text(
        yaml_sequence_depth_50(), encoding="utf-8"
    )
    (args.yaml_dir / "blackboard_writes.yaml").write_text(
        yaml_blackboard_writes(), encoding="utf-8"
    )
    (args.yaml_dir / "blackboard_port_reads.yaml").write_text(
        yaml_blackboard_port_reads(), encoding="utf-8"
    )
    (args.yaml_dir / "blackboard_subtree_remap.yaml").write_text(
        yaml_blackboard_subtree_remap(), encoding="utf-8"
    )
    (args.yaml_dir / "blackboard_large.yaml").write_text(
        yaml_blackboard_large(), encoding="utf-8"
    )

    (args.xml_dir / "parallel_100_success.xml").write_text(
        xml_parallel_100_success(), encoding="utf-8"
    )
    (args.xml_dir / "sequence_depth_50.xml").write_text(
        xml_sequence_depth_50(), encoding="utf-8"
    )
    (args.xml_dir / "patrol.xml").write_text(xml_patrol(), encoding="utf-8")
    (args.xml_dir / "game_state.xml").write_text(xml_game_state(), encoding="utf-8")
    (args.xml_dir / "blackboard_writes.xml").write_text(
        xml_blackboard_writes(), encoding="utf-8"
    )
    (args.xml_dir / "blackboard_port_reads.xml").write_text(
        xml_blackboard_port_reads(), encoding="utf-8"
    )
    (args.xml_dir / "blackboard_subtree_remap.xml").write_text(
        xml_blackboard_subtree_remap(), encoding="utf-8"
    )
    (args.xml_dir / "blackboard_large.xml").write_text(
        xml_blackboard_large(), encoding="utf-8"
    )

    print(f"Generated YAML in {args.yaml_dir}")
    print(f"Generated XML  in {args.xml_dir}")


if __name__ == "__main__":
    main()
