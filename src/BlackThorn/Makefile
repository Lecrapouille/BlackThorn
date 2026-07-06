# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. It is available under the GNU GPL v3 or,
# for users who cannot use the GPL, under a commercial license.
# See LICENSING.md for details.

## BlackThorn: A behavior tree library.

###############################################################################
# Location of the project directory and Makefiles
#
P := ../..
M := $(P)/.makefile
CURRENT_DIR := $(P)/src/BlackThorn

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := blackthorn
TARGET_DESCRIPTION := Library for BlackThorn Behavior Tree
include $(M)/project/Makefile
include $(CURRENT_DIR)/BlackThorn.deps.mk

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(P)/src/ $(THIRD_PARTIES_DIR)

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH +=  $(P)/src/

###############################################################################
# Project defines
#
DEFINES +=

###############################################################################
# Make the list of files to compile
#
LIB_FILES += $(call rwildcard,$(CURRENT_DIR),*.cpp)
HEADER_FILES := $(call rwildcard,$(CURRENT_DIR),*.hpp)

###############################################################################
# Set Libraries
#
PKG_LIBS += yaml-cpp sfml-network

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile
