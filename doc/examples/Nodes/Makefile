###############################################################################
## Behavior Tree: A behavior tree library.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of Behavior Tree.
##
## Behavior Tree is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################

###############################################################################
# Location of the project directory and Makefiles
#
P := ../../..
M := $(P)/.makefile

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := Example-Nodes
TARGET_DESCRIPTION := Example of nodes behavior tree
include $(M)/project/Makefile

CURRENT_DIR := $(P)/doc/examples/Nodes

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
INTERNAL_LIBS += $(call internal-lib,blackthorn)

###############################################################################
# Make the list of files to compile
#
SRC_FILES := $(call rwildcard,$(CURRENT_DIR),*.cpp)

###################################################
# Set Libraries.
#
PKG_LIBS += yaml-cpp sfml-network

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile