###############################################################################
## BlackThorn benchmarks
## Copyright 2025 Quentin Quadrat <lecrapouille@gmail.com>
## distributed under MIT License
###############################################################################

P := ..
M := $(P)/.makefile

include $(P)/Makefile.common
TARGET_NAME := $(PROJECT_NAME)-Benchmark
TARGET_DESCRIPTION := Comparative benchmarks for $(PROJECT_NAME)
COMPILATION_MODE := release
include $(M)/project/Makefile
include $(P)/src/BlackThorn/BlackThorn.deps.mk

CURRENT_DIR := $(P)/benchmarks
BTCPP_DIR := $(P)/external/BehaviorTree.CPP
BTCPP_LIB := $(BTCPP_DIR)/build/libbehaviortree_cpp.a

INCLUDES += $(CURRENT_DIR) $(P)/src $(BTCPP_DIR)/include $(THIRD_PARTIES_DIR)
VPATH += $(CURRENT_DIR)

INTERNAL_LIBS += $(call internal-lib,blackthorn)
PKG_LIBS += yaml-cpp sfml-network
NOT_PKG_LIBS += $(BTCPP_LIB) -lpthread -dl

SRC_FILES := $(CURRENT_DIR)/main.cpp

YAML_DIR := $(CURRENT_DIR)/yaml
XML_DIR := $(CURRENT_DIR)/xml
GENERATED_YAML := $(YAML_DIR)/parallel_100_success.yaml $(YAML_DIR)/sequence_depth_50.yaml \
                  $(YAML_DIR)/blackboard_writes.yaml $(YAML_DIR)/blackboard_port_reads.yaml \
                  $(YAML_DIR)/blackboard_subtree_remap.yaml $(YAML_DIR)/blackboard_large.yaml
GENERATED_XML := $(XML_DIR)/parallel_100_success.xml $(XML_DIR)/sequence_depth_50.xml \
                 $(XML_DIR)/patrol.xml $(XML_DIR)/game_state.xml \
                 $(XML_DIR)/blackboard_writes.xml $(XML_DIR)/blackboard_port_reads.xml \
                 $(XML_DIR)/blackboard_subtree_remap.xml $(XML_DIR)/blackboard_large.xml

.PHONY: yaml xml bt-cpp-lib
yaml: $(GENERATED_YAML)
xml: $(GENERATED_XML)

$(GENERATED_YAML) $(GENERATED_XML): $(CURRENT_DIR)/generate_yaml.py
	@python3 $(CURRENT_DIR)/generate_yaml.py --yaml-dir $(YAML_DIR) --xml-dir $(XML_DIR)

$(BTCPP_LIB):
	@$(MAKE) -C $(P) compile-external-libs

$(TARGET_BINARY_NAME): yaml xml $(BTCPP_LIB)

include $(M)/rules/Makefile
