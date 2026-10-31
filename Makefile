# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. It is available under the GNU GPL v3 or,
# for users who cannot use the GPL, under a commercial license.
# See LICENSING.md for details.

## Behavior Tree: A behavior tree library.

PROJECT_NAME := BlackThorn
PROJECT_VERSION := 0.1.0
COMPILATION_MODE ?= release
CXX_STANDARD := c++17

###############################################################################
# The libraries (BlackThorn.mk, also read by the projects embedding them),
# their third parties, the editor, the tests and the examples. The
# benchmarks build BehaviorTree.CPP: they are declared only for
# 'make benchmark' and 'make benchmark-save'.
#
SUBMAKEFILES := BlackThorn.mk external/external.mk applications/Oakular/Oakular.mk \
    tests/tests.mk doc/examples/examples.mk
ifneq ($(filter benchmark benchmark-save BlackThorn-Benchmark,$(MAKECMDGOALS)),)
    SUBMAKEFILES += benchmarks/benchmarks.mk
endif

M ?= .makefile
include $(M)/MyMakefile.mk
