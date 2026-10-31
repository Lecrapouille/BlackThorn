# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# The libraries blackthorn (behavior trees) and oakular (their editor), for
# MyMakefile 3. A project embedding them reads this file with their third
# parties (rapidyaml, and Dear ImGui unless the project declares its own):
#   TARGETS += BlackThorn
#   BlackThorn.GIT := https://github.com/Lecrapouille/BlackThorn.git
#   BlackThorn.MAKEFILES := BlackThorn.mk external/external.mk
#   myapp.DEPENDS := blackthorn        # or oakular, for the editor

#? 0: no network (SFML) in blackthorn: a tree cannot send its state.
BLACKTHORN_WITH_NETWORK ?= 1
#? 0: no server in oakular: no visualizer mode.
OAKULAR_WITH_SERVER ?= 1
ifeq ($(TARGET_OS),Emscripten)
    BLACKTHORN_WITH_NETWORK := 0
    OAKULAR_WITH_SERVER := 0
endif

TARGETS += blackthorn oakular

###############################################################################
# The behavior tree library, with rapidyaml in it.
#
blackthorn.TYPE := library
blackthorn.DESCRIPTION := Library for BlackThorn Behavior Tree
blackthorn.SOURCES := src/BlackThorn/Blackboard/**/*.cpp src/BlackThorn/Builder/**/*.cpp \
    src/BlackThorn/Nodes/**/*.cpp
blackthorn.PUBLIC_INCLUDES := src
blackthorn.DEPENDS := rapidyaml
blackthorn.MERGE := rapidyaml
ifeq ($(BLACKTHORN_WITH_NETWORK),1)
    blackthorn.SOURCES += src/BlackThorn/Network/**/*.cpp
    blackthorn.PUBLIC_DEFINES := BLACKTHORN_HAS_NETWORK=1
    blackthorn.PKG_CONFIG := sfml-network
endif

###############################################################################
# The library of the editor. It uses the headers of Dear ImGui only: the
# targets depending on it link the target imgui, the one of the project when
# it declares its own, so that the final binary has one ImGui.
#
oakular.TYPE := library
oakular.DESCRIPTION := Oakular - Behavior tree editor library
oakular.SOURCES := src/Oakular/Editor.cpp src/Oakular/EditorWidgets.cpp src/Oakular/TreeRenderer.cpp
oakular.PUBLIC_INCLUDES := src/Oakular
oakular.DEPENDS := blackthorn imgui
oakular.CXXFLAGS := -Wno-conversion -Wno-arith-conversion -Wno-double-promotion \
    -Wno-cast-qual -Wno-old-style-cast -Wno-sign-conversion -Wno-duplicated-branches \
    -Wno-useless-cast -Wno-ctor-dtor-privacy -Wno-float-equal -Wno-null-dereference
ifeq ($(OAKULAR_WITH_SERVER),1)
    oakular.SOURCES += src/Oakular/Server.cpp
    oakular.PUBLIC_DEFINES := OAKULAR_HAS_SERVER=1
    oakular.PKG_CONFIG := sfml-network
endif
