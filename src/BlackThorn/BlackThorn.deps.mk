###############################################################################
## Shared third-party dependencies for BlackThorn consumers.
###############################################################################

RYML_DIR := $(THIRD_PARTIES_DIR)/rapidyaml
INCLUDES += -isystem $(RYML_DIR)/src -isystem $(RYML_DIR)/ext/c4core/src
THIRD_PARTIES_LIBS += $(RYML_DIR)/build/libryml.a

# Link executables against static archives (no runtime libblackthorn.so needed).
BLACKTHORN_STATIC_LIB := $(BUILD_PATH)/libblackthorn.a
BLACKTHORN_LINK_LIBS := $(BLACKTHORN_STATIC_LIB) $(THIRD_PARTIES_LIBS)

$(BLACKTHORN_STATIC_LIB):
	@$(MAKE) -C $(P)/src/BlackThorn all
