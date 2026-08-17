###############################################################################
## BlackThorn: A behavior tree library.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of BlackThorn.
##
## BlackThorn is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################
##
## This example stands in for a host project embedding BlackThorn and Oakular.
## It deliberately consumes them through BlackThorn.mk, exactly as an external
## project would, instead of through the internal BlackThorn.deps.mk. It also
## deliberately does not link the Application layer of the standalone Oakular
## application: the whole point is to prove the editor works with a window and
## an ImGui context owned by the host.
##
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
TARGET_NAME := Example-Embedded
TARGET_DESCRIPTION := Example embedding the Oakular editor in a host window
include $(M)/project/Makefile

CURRENT_DIR := $(P)/doc/examples/Embedded

###############################################################################
# Consume BlackThorn the way an external host does.
#
BLACKTHORN_DIR := $(abspath $(P))
include $(BLACKTHORN_DIR)/BlackThorn.mk

INCLUDES += $(CURRENT_DIR) $(BLACKTHORN_INCLUDES) $(OAKULAR_INCLUDES)
VPATH += $(CURRENT_DIR) $(BLACKTHORN_VPATH) $(OAKULAR_VPATH)
DEFINES += $(BLACKTHORN_DEFINES) $(OAKULAR_DEFINES)
PKG_LIBS += $(BLACKTHORN_PKG_LIBS) $(OAKULAR_PKG_LIBS)
USER_CXXFLAGS += $(BLACKTHORN_CXXFLAGS) $(OAKULAR_CXXFLAGS)

###############################################################################
# Make the list of files to compile: the host sources, the two BlackThorn
# layers, and the single copy of Dear ImGui the host is responsible for.
#
SRC_FILES := $(CURRENT_DIR)/Embedded.cpp
SRC_FILES += $(BLACKTHORN_SOURCES) $(OAKULAR_SOURCES)

VPATH += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
SRC_FILES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp
SRC_FILES += $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SRC_FILES += $(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp
SRC_FILES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp
SRC_FILES += $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

###################################################
# Set third-party libraries. Order matters: dependencies last
#
INTERNAL_LIBS :=
THIRD_PARTIES_LIBS :=
NOT_PKG_LIBS += $(BLACKTHORN_STATIC_DEPS)

###################################################
# Set Libraries. The window and the OpenGL context belong to the host.
#
PKG_LIBS += glfw3 glew gl

###############################################################################
# Generic Makefile rules
#
include $(M)/rules/Makefile
