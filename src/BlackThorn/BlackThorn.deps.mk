###############################################################################
## Shared third-party dependencies for BlackThorn consumers.
###############################################################################

RYML_DIR := $(THIRD_PARTIES_DIR)/rapidyaml
INCLUDES += $(RYML_DIR)/src $(RYML_DIR)/ext/c4core/src
THIRD_PARTIES_LIBS += $(RYML_DIR)/build/libryml.a
