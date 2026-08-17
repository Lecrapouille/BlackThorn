# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. It is available under the GNU GPL v3 or,
# for users who cannot use the GPL, under a commercial license.
# See LICENSING.md for details.

## Behavior Tree: A behavior tree library.

###############################################################################
# Location of the project directory and Makefiles
#
P := ../../..
M := $(P)/.makefile

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := Example-GameState
TARGET_DESCRIPTION := Example of a game state behavior tree
include $(M)/project/Makefile
include $(P)/src/BlackThorn/BlackThorn.deps.mk

CURRENT_DIR := $(P)/doc/examples/GameState

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(CURRENT_DIR) $(THIRD_PARTIES_DIR) $(P)/src

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH += $(P)/src $(CURRENT_DIR)

###################################################
# Set third-party libraries. Order matters: dependencies last
#
NOT_PKG_LIBS += $(BLACKTHORN_LINK_LIBS)

###############################################################################
# Make the list of files to compile
#
SRC_FILES := $(call rwildcard,$(CURRENT_DIR),*.cpp)

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile

$(TARGET_BINARY_NAME): $(BLACKTHORN_STATIC_LIB)
