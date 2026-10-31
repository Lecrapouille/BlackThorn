# SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
# Copyright (c) 2025-2026 Quentin Quadrat
#
# This file is part of BlackThorn. See LICENSING.md for details.
#
# The examples, one application per folder. Paths are relative to this
# directory. They read their YAML file from the root of the project.

EXAMPLES := GameState Nodes Patrol StatusDemo SubTreeRemapping
TARGETS += $(addprefix Example-,$(EXAMPLES)) Example-Embedded

$(foreach e,$(EXAMPLES),$(eval Example-$e.SOURCES := $e/*.cpp))
$(foreach e,$(EXAMPLES),$(eval Example-$e.INCLUDES := $e))
$(foreach e,$(EXAMPLES),$(eval Example-$e.DEPENDS := blackthorn))
$(foreach e,$(EXAMPLES),$(eval Example-$e.INSTALL := no))

# The Oakular editor embedded in a host window.
Example-Embedded.DESCRIPTION := Example embedding the Oakular editor in a host window
Example-Embedded.SOURCES := Embedded/*.cpp
Example-Embedded.INCLUDES := Embedded
Example-Embedded.DEPENDS := oakular
Example-Embedded.CXXFLAGS := $(oakular.CXXFLAGS)
Example-Embedded.INSTALL := no
ifneq ($(TARGET_OS),Emscripten)
    Example-Embedded.PKG_CONFIG := glfw3 glew gl
endif
