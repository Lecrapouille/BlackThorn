###############################################################################
## Oakular: Behavior tree editor application using the Oakular library.
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
TARGET_NAME := Oakular
TARGET_DESCRIPTION := Oakular - Behavior tree editor and real-time visualizer
include $(M)/project/Makefile
include $(P)/src/BlackThorn/BlackThorn.deps.mk
include $(P)/src/Oakular/Oakular.deps.mk

CURRENT_DIR := $(P)/applications/Oakular
LIBRARY_DIR := $(P)/src/BlackThorn
OAKULAR_LIB_DIR := $(P)/src/Oakular

###############################################################################
# Inform Makefile where to find header files
#
INCLUDES += $(CURRENT_DIR) $(P)/src $(THIRD_PARTIES_DIR) $(OAKULAR_LIB_DIR)

###############################################################################
# Inform Makefile where to find *.cpp files
#
VPATH += $(CURRENT_DIR) $(LIBRARY_DIR)

###################################################
# Set third-party libraries. Order matters: dependencies last
#
INTERNAL_LIBS :=
NOT_PKG_LIBS += $(OAKULAR_STATIC_LIB) $(BLACKTHORN_LINK_LIBS)

###############################################################################
# Make the list of files to compile (application sources only)
#
SRC_FILES := $(CURRENT_DIR)/main.cpp
SRC_FILES += $(CURRENT_DIR)/OakularApp.cpp
SRC_FILES += $(CURRENT_DIR)/Application/Application.cpp
SRC_FILES += $(CURRENT_DIR)/Application/DearImGuiApplication.cpp

###################################################
# Compile Dear ImGui. As the host of the editor, this application owns the
# single copy of ImGui linked in the final binary, backends included.
#
VPATH += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
SRC_FILES += $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/imgui_draw.cpp
SRC_FILES += $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui.cpp
SRC_FILES += $(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp
SRC_FILES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp
SRC_FILES += $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

###################################################
# Compile third party ImGuiFileDialog. The file browser is a choice of the host
# application, not of the editor library.
#
IMGUIFILEDIALOG_DIR ?= $(THIRD_PARTIES_DIR)/ImGuiFileDialog
INCLUDES += $(IMGUIFILEDIALOG_DIR)
VPATH += $(IMGUIFILEDIALOG_DIR)
SRC_FILES += $(IMGUIFILEDIALOG_DIR)/ImGuiFileDialog.cpp

###################################################
# Set Libraries. The window and OpenGL context belong to this application.
#
PKG_LIBS += glfw3 glew gl

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile

$(TARGET_BINARY_NAME): $(OAKULAR_STATIC_LIB) $(BLACKTHORN_STATIC_LIB)
