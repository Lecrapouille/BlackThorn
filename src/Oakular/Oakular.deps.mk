###############################################################################
## Shared dependencies for Oakular consumers.
##
## Included by the editor library itself and by every host embedding it, so that
## the compile flags stay consistent on both sides.
##
## Dear ImGui is deliberately absent from the sources listed here: the host owns
## the ImGui context and compiles ImGui once, so that a single copy of it ends
## up in the final binary. Override IMGUI_DIR to point at your own clone.
###############################################################################

IMGUI_DIR ?= $(THIRD_PARTIES_DIR)/imgui
INCLUDES += $(IMGUI_DIR) $(IMGUI_DIR)/misc/cpp $(IMGUI_DIR)/backends

# Optional TCP server feeding the visualizer mode. This flag drives what
# Oakular.hpp exposes and whether oakular::Editor knows about a server, so a
# host must see the very same value as the library it links against.
ifeq ($(OAKULAR_WITH_SERVER),1)
DEFINES += -DOAKULAR_HAS_SERVER=1
PKG_LIBS += sfml-network
endif

# ImGui triggers a lot of noise with the strict warning set of MyMakefile.
USER_CXXFLAGS += -Wno-conversion -Wno-arith-conversion -Wno-double-promotion -Wno-cast-qual
USER_CXXFLAGS += -Wno-old-style-cast -Wno-sign-conversion -Wno-duplicated-branches
USER_CXXFLAGS += -Wno-useless-cast -Wno-ctor-dtor-privacy -Wno-float-equal -Wno-null-dereference

OAKULAR_STATIC_LIB := $(BUILD_PATH)/liboakular.a

# Convenience rule letting a consumer build the archive on demand. Skipped when
# the editor library is building itself, as MyMakefile already provides a recipe
# for its own target and overriding it would warn.
ifneq ($(TARGET_NAME),oakular)
$(OAKULAR_STATIC_LIB):
	@$(MAKE) -C $(P)/src/Oakular all
endif
