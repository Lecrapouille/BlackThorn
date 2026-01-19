###############################################################################
## SubTreeRemapping: Example demonstrating SubTree port remapping
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
###############################################################################

P := ../../..
M := $(P)/.makefile

include $(P)/Makefile.common
TARGET_NAME := Example-SubTreeRemapping
TARGET_DESCRIPTION := Example demonstrating SubTree port remapping
include $(M)/project/Makefile

CURRENT_DIR := $(P)/doc/examples/SubTreeRemapping
LIBRARY_DIR := $(P)/src/BlackThorn

INCLUDES += $(P)/src $(THIRD_PARTIES_DIR)
VPATH += $(CURRENT_DIR) $(LIBRARY_DIR)

INTERNAL_LIBS += $(call internal-lib,blackthorn)
SRC_FILES := $(CURRENT_DIR)/SubTreeRemapping.cpp
PKG_LIBS += yaml-cpp sfml-network

include $(M)/rules/Makefile
