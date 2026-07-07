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

# Link pre-built libs instead of merging their .o into liboakular.a (broken
# with parallel builds: object dirs live under objs-Linux/blackthorn, not
# objs-Linux/oakular/libblackthorn).
THIRD_PARTIES_LIBS :=
INTERNAL_LIBS :=
NOT_PKG_LIBS += -L$(P)/build -lblackthorn

$(TARGET_STATIC_LIB_NAME) $(TARGET_SHARED_LIB_NAME): $(P)/build/libblackthorn.a

###############################################################################
# Inform Makefile where to find *.cpp files
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
LIB_FILES := $(CURRENT_DIR)/IDE.cpp
LIB_FILES += $(CURRENT_DIR)/Renderer.cpp
LIB_FILES += $(CURRENT_DIR)/Server.cpp
LIB_FILES += $(CURRENT_DIR)/Application/Application.cpp
LIB_FILES += $(CURRENT_DIR)/Application/DearImGuiApplication.cpp

###################################################
# Set third party Dear ImGui
#
IMGUI_DIR := $(THIRD_PARTIES_DIR)/imgui
INCLUDES += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
VPATH += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
LIB_FILES += $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/imgui_draw.cpp
LIB_FILES += $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui.cpp
LIB_FILES += $(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp
LIB_FILES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp
LIB_FILES += $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
USER_CXXFLAGS += -Wno-conversion -Wno-arith-conversion -Wno-double-promotion -Wno-cast-qual
USER_CXXFLAGS += -Wno-old-style-cast -Wno-sign-conversion -Wno-duplicated-branches
USER_CXXFLAGS += -Wno-useless-cast -Wno-ctor-dtor-privacy -Wno-float-equal -Wno-null-dereference

###################################################
# Set third party ImGuiFileDialog
#
IMGUIFILEDIALOG_DIR := $(THIRD_PARTIES_DIR)/ImGuiFileDialog
INCLUDES += $(IMGUIFILEDIALOG_DIR)
VPATH += $(IMGUIFILEDIALOG_DIR)
LIB_FILES += $(IMGUIFILEDIALOG_DIR)/ImGuiFileDialog.cpp

###################################################
# Set Libraries.
#
PKG_LIBS += sfml-network glfw3 glew gl

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile
