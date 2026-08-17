# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. It is available under the GNU GPL v3 or,
# for users who cannot use the GPL, under a commercial license.
# See LICENSING.md for details.

## SubTreeRemapping: Example demonstrating SubTree port remapping

P := ../../..
M := $(P)/.makefile

include $(P)/Makefile.common
TARGET_NAME := Example-SubTreeRemapping
TARGET_DESCRIPTION := Example demonstrating SubTree port remapping
include $(M)/project/Makefile
include $(P)/src/BlackThorn/BlackThorn.deps.mk

CURRENT_DIR := $(P)/doc/examples/SubTreeRemapping
LIBRARY_DIR := $(P)/src/BlackThorn

INCLUDES += $(P)/src $(THIRD_PARTIES_DIR)
VPATH += $(CURRENT_DIR) $(LIBRARY_DIR)

NOT_PKG_LIBS += $(BLACKTHORN_LINK_LIBS)
SRC_FILES := $(CURRENT_DIR)/SubTreeRemapping.cpp

include $(M)/rules/Makefile

$(TARGET_BINARY_NAME): $(BLACKTHORN_STATIC_LIB)
