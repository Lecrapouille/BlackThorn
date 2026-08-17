###############################################################################
## Oakular: Behavior tree editor library for BlackThorn.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of BlackThorn.
##
## BlackThorn is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################

###############################################################################
# Location of the project directory and Makefiles
#
P := ../..
M := $(P)/.makefile

###############################################################################
# Project definition
#
include $(P)/Makefile.common
TARGET_NAME := oakular
TARGET_DESCRIPTION := Oakular - Behavior tree editor library
CURRENT_DIR := $(P)/src/Oakular
LIBRARY_DIR := $(P)/src/BlackThorn
include $(M)/project/Makefile
include $(LIBRARY_DIR)/BlackThorn.deps.mk
include $(CURRENT_DIR)/Oakular.deps.mk

# Link pre-built libs instead of merging their .o into liboakular.a (broken
# with parallel builds: object dirs live under objs-Linux/blackthorn, not
# objs-Linux/oakular/libblackthorn).
THIRD_PARTIES_LIBS :=
INTERNAL_LIBS :=
NOT_PKG_LIBS += -L$(P)/build -lblackthorn

$(TARGET_STATIC_LIB_NAME) $(TARGET_SHARED_LIB_NAME): $(P)/build/libblackthorn.a

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(CURRENT_DIR) $(P)/src $(THIRD_PARTIES_DIR)

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH += $(CURRENT_DIR) $(LIBRARY_DIR)

###################################################
# Set third-party libraries. Order matters: dependencies last
#
INTERNAL_LIBS += $(call internal-lib,blackthorn)

###############################################################################
# Make the list of library files to compile (no main.cpp)
#
LIB_FILES := $(CURRENT_DIR)/Editor.cpp
LIB_FILES += $(CURRENT_DIR)/EditorWidgets.cpp
LIB_FILES += $(CURRENT_DIR)/TreeRenderer.cpp

###############################################################################
# Optional TCP server feeding the visualizer mode. The define and the
# sfml-network dependency come from Oakular.deps.mk, shared with the hosts.
#
ifeq ($(OAKULAR_WITH_SERVER),1)
LIB_FILES += $(CURRENT_DIR)/Server.cpp
endif

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile
