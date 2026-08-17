###############################################################################
## BlackThorn: integration fragment for host projects.
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
##
## This file is part of BlackThorn.
##
## BlackThorn is free software: you can redistribute it and/or modify it
## under the terms of the MIT License.
###############################################################################
##
## Include this file from the Makefile of a project embedding BlackThorn and/or
## its editor Oakular. Unlike src/BlackThorn/BlackThorn.deps.mk, which is
## internal and assumes the layout of this repository, this fragment is
## self-contained: it only needs BLACKTHORN_DIR.
##
## Usage, from a host already compiling Dear ImGui (see IMGUI_DIR below):
##
##   BLACKTHORN_DIR := $(P)/external/BlackThorn
##   IMGUI_DIR := $(P)/external/imgui
##   include $(BLACKTHORN_DIR)/BlackThorn.mk
##
##   INCLUDES += $(BLACKTHORN_INCLUDES) $(OAKULAR_INCLUDES)
##   VPATH += $(BLACKTHORN_VPATH) $(OAKULAR_VPATH)
##   LIB_FILES += $(BLACKTHORN_SOURCES) $(OAKULAR_SOURCES)
##   PKG_LIBS += $(BLACKTHORN_PKG_LIBS) $(OAKULAR_PKG_LIBS)
##   DEFINES += $(BLACKTHORN_DEFINES) $(OAKULAR_DEFINES)
##   USER_CXXFLAGS += $(BLACKTHORN_CXXFLAGS) $(OAKULAR_CXXFLAGS)
##
## The *_INCLUDES variables hold bare directories, to be prefixed with -I by the
## build system of the host. The *_CXXFLAGS ones hold ready-to-use compiler
## flags.
##
## Drop the OAKULAR_* variables to embed the behavior tree engine alone, without
## its graphical editor. In that case Dear ImGui is not needed at all.
##
###############################################################################

ifndef BLACKTHORN_DIR
$(error BlackThorn.mk: define BLACKTHORN_DIR before including this file)
endif

###############################################################################
# Optional features. Keep these values identical to the ones used to build the
# pre-compiled archives, should you link them rather than compile the sources:
# they drive what the public headers expose.
#
# Compile bt::VisualizerClient, streaming the runtime state of a tree to the
# Oakular visualizer. Requires sfml-network.
BLACKTHORN_WITH_NETWORK ?= 1
# Compile oakular::Server, the TCP server feeding the visualizer mode of the
# editor. Requires sfml-network.
OAKULAR_WITH_SERVER ?= 1

###############################################################################
# rapidyaml, the YAML backend of BlackThorn. Override RYML_DIR to reuse a clone
# already present in the host project.
#
RYML_DIR ?= $(BLACKTHORN_DIR)/external/rapidyaml

###############################################################################
# Dear ImGui. Oakular consumes its headers only: the host compiles ImGui and
# owns the ImGui context, so that a single copy of it ends up in the final
# binary. The host must compile at least:
#   imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp
#   misc/cpp/imgui_stdlib.cpp
# plus the backends of its choice.
#
IMGUI_DIR ?= $(BLACKTHORN_DIR)/external/imgui

###############################################################################
# Layer 1: the behavior tree engine. Headers are reached as
# "BlackThorn/Nodes/Tree.hpp", hence the src/ include root.
#
BLACKTHORN_INCLUDES := $(BLACKTHORN_DIR)/src
BLACKTHORN_VPATH := $(BLACKTHORN_DIR)/src
BLACKTHORN_DEFINES :=
BLACKTHORN_PKG_LIBS :=

# rapidyaml is private: the public headers only forward declare it, so these
# flags are needed to compile BLACKTHORN_SOURCES, never to include the headers.
# -isystem keeps the warnings of the vendored headers quiet.
BLACKTHORN_CXXFLAGS := -isystem $(abspath $(RYML_DIR)/src)
BLACKTHORN_CXXFLAGS += -isystem $(abspath $(RYML_DIR)/ext/c4core/src)

BLACKTHORN_SOURCES := $(filter-out $(BLACKTHORN_DIR)/src/BlackThorn/Network/%, \
    $(shell find $(BLACKTHORN_DIR)/src/BlackThorn -name '*.cpp' 2> /dev/null))

ifeq ($(BLACKTHORN_WITH_NETWORK),1)
BLACKTHORN_DEFINES += -DBLACKTHORN_HAS_NETWORK=1
BLACKTHORN_PKG_LIBS += sfml-network
BLACKTHORN_SOURCES += $(shell find $(BLACKTHORN_DIR)/src/BlackThorn/Network -name '*.cpp' 2> /dev/null)
endif

# rapidyaml ships as a CMake project: point at the archive it produces.
BLACKTHORN_STATIC_DEPS := $(RYML_DIR)/build/libryml.a

###############################################################################
# Layer 2: the graphical editor. Headers are reached as "Oakular/Editor.hpp",
# hence the same src/ include root.
#
OAKULAR_INCLUDES := $(BLACKTHORN_DIR)/src
OAKULAR_INCLUDES += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends
OAKULAR_VPATH := $(BLACKTHORN_DIR)/src
OAKULAR_DEFINES :=
OAKULAR_PKG_LIBS :=

OAKULAR_SOURCES := $(BLACKTHORN_DIR)/src/Oakular/Editor.cpp
OAKULAR_SOURCES += $(BLACKTHORN_DIR)/src/Oakular/EditorWidgets.cpp
OAKULAR_SOURCES += $(BLACKTHORN_DIR)/src/Oakular/TreeRenderer.cpp

ifeq ($(OAKULAR_WITH_SERVER),1)
OAKULAR_DEFINES += -DOAKULAR_HAS_SERVER=1
OAKULAR_PKG_LIBS += sfml-network
OAKULAR_SOURCES += $(BLACKTHORN_DIR)/src/Oakular/Server.cpp
endif

# Dear ImGui triggers a lot of noise under a strict warning set. Apply these to
# your whole target, or restrict them to the Oakular objects if your build
# system allows per-file flags.
OAKULAR_CXXFLAGS := -Wno-conversion -Wno-arith-conversion -Wno-double-promotion
OAKULAR_CXXFLAGS += -Wno-cast-qual -Wno-old-style-cast -Wno-sign-conversion
OAKULAR_CXXFLAGS += -Wno-duplicated-branches -Wno-useless-cast
OAKULAR_CXXFLAGS += -Wno-ctor-dtor-privacy -Wno-float-equal -Wno-null-dereference

###############################################################################
# Alternative to compiling the sources: link the archives built by
# `make -C $(BLACKTHORN_DIR)`. Both layers are static, so nothing needs to be
# deployed at runtime.
#
BLACKTHORN_ARCHIVES := $(BLACKTHORN_DIR)/build/libblackthorn.a
BLACKTHORN_ARCHIVES += $(BLACKTHORN_STATIC_DEPS)
OAKULAR_ARCHIVES := $(BLACKTHORN_DIR)/build/liboakular.a
