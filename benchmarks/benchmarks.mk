# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# Comparative benchmarks against BehaviorTree.CPP: 'make benchmark' builds
# and runs them, 'make benchmark-save' keeps the results. Paths are
# relative to this directory.

TARGETS += BehaviorTree.CPP BlackThorn-Benchmark

### BehaviorTree.CPP, built by its CMake. License: MIT
BehaviorTree.CPP.GIT := https://github.com/BehaviorTree/BehaviorTree.CPP.git
BehaviorTree.CPP.VERSION := 4.9.0
BehaviorTree.CPP.TYPE := cmake
BehaviorTree.CPP.CMAKE_ARGS := -DBTCPP_SHARED_LIBS=OFF -DBTCPP_BUILD_TOOLS=OFF \
    -DBTCPP_EXAMPLES=OFF -DBTCPP_UNIT_TESTS=OFF -DBTCPP_GROOT_INTERFACE=OFF \
    -DBTCPP_SQLITE_LOGGING=OFF -DBUILD_TESTING=OFF
# Its tinyxml2 is not installed: taken from its build, beside the prefix.
BehaviorTree.CPP.LIBRARIES := behaviortree_cpp ../../build/3rdparty/tinyxml2/libtinyxml2.a

BlackThorn-Benchmark.DESCRIPTION := Comparative benchmarks for BlackThorn
BlackThorn-Benchmark.SOURCES := main.cpp
BlackThorn-Benchmark.INCLUDES := .
BlackThorn-Benchmark.DEPENDS := blackthorn BehaviorTree.CPP
BlackThorn-Benchmark.LIBS := -lpthread -ldl
BlackThorn-Benchmark.INSTALL := no

# The trees of the benchmarks, made by generate_yaml.py.
BENCHMARK_DIR := $(HERE)
BENCHMARK_TREES := $(addprefix $(HERE)yaml/,parallel_100_success.yaml sequence_depth_50.yaml \
    blackboard_writes.yaml blackboard_port_reads.yaml blackboard_subtree_remap.yaml blackboard_large.yaml) \
    $(addprefix $(HERE)xml/,parallel_100_success.xml sequence_depth_50.xml patrol.xml game_state.xml \
    blackboard_writes.xml blackboard_port_reads.xml blackboard_subtree_remap.xml blackboard_large.xml)

$(BENCHMARK_TREES) &: $(HERE)generate_yaml.py
	@python3 $(BENCHMARK_DIR)generate_yaml.py --yaml-dir $(BENCHMARK_DIR)yaml --xml-dir $(BENCHMARK_DIR)xml

.PHONY: benchmark benchmark-save
benchmark: BlackThorn-Benchmark $(BENCHMARK_TREES)
	@$(BlackThorn-Benchmark._EXE)

benchmark-save: BlackThorn-Benchmark $(BENCHMARK_TREES)
	@BIN=$(BlackThorn-Benchmark._EXE) $(BENCHMARK_DIR)save_results.sh
