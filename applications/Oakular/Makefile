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

###############################################################################
# Inform Makefile where to find *.cpp files
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
INTERNAL_LIBS += $(call internal-lib,oakular)
INTERNAL_LIBS += $(call internal-lib,blackthorn)

###############################################################################
# Make the list of files to compile (application sources only)
#
SRC_FILES := $(CURRENT_DIR)/main.cpp
SRC_FILES += $(CURRENT_DIR)/OakularApp.cpp

###################################################
# Set third party Dear ImGui (for headers)
#
IMGUI_DIR := $(THIRD_PARTIES_DIR)/imgui
INCLUDES += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
USER_CXXFLAGS += -Wno-conversion -Wno-arith-conversion -Wno-double-promotion -Wno-cast-qual
USER_CXXFLAGS += -Wno-old-style-cast -Wno-sign-conversion -Wno-duplicated-branches
USER_CXXFLAGS += -Wno-useless-cast -Wno-ctor-dtor-privacy -Wno-float-equal -Wno-null-dereference

###################################################
# Set third party ImGuiFileDialog (for headers)
#
IMGUIFILEDIALOG_DIR := $(THIRD_PARTIES_DIR)/ImGuiFileDialog
INCLUDES += $(IMGUIFILEDIALOG_DIR)

###################################################
# Set Libraries.
#
PKG_LIBS += yaml-cpp sfml-network glfw3 glew gl

###############################################################################
# Sharable information between all Makefiles
#
include $(M)/rules/Makefile
