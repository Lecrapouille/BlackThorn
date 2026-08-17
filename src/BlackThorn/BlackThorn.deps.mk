###############################################################################
## Shared third-party dependencies for BlackThorn consumers.
##
## Included by the library itself and by everything linking against it, so that
## the compile flags stay consistent on both sides.
###############################################################################

RYML_DIR ?= $(THIRD_PARTIES_DIR)/rapidyaml
THIRD_PARTIES_LIBS += $(RYML_DIR)/build/libryml.a

# Only Yaml.cpp includes rapidyaml: the public headers merely forward declare
# it. Passed as -isystem so the vendored headers stop reporting warnings, hence
# through USER_CXXFLAGS and not INCLUDES, which only accepts bare paths (each
# word gets a -I prefix).
USER_CXXFLAGS += -isystem $(abspath $(RYML_DIR)/src)
USER_CXXFLAGS += -isystem $(abspath $(RYML_DIR)/ext/c4core/src)

# Optional network support. This flag drives what BlackThorn.hpp exposes, so a
# consumer must see the very same value as the library it links against.
ifeq ($(BLACKTHORN_WITH_NETWORK),1)
DEFINES += -DBLACKTHORN_HAS_NETWORK=1
PKG_LIBS += sfml-network
endif

# Link executables against static archives (no runtime libblackthorn.so needed).
BLACKTHORN_STATIC_LIB := $(BUILD_PATH)/libblackthorn.a
BLACKTHORN_LINK_LIBS := $(BLACKTHORN_STATIC_LIB) $(THIRD_PARTIES_LIBS)

# Convenience rule letting a consumer build the archive on demand. Skipped when
# the library is building itself, as MyMakefile already provides a recipe for
# its own target and overriding it would warn.
ifneq ($(TARGET_NAME),blackthorn)
$(BLACKTHORN_STATIC_LIB):
	@$(MAKE) -C $(P)/src/BlackThorn all
endif
