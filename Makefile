ARCH ?= $(shell gcc --print-multiarch)
ROOT_DIR ?= $(shell pwd)
BUILD_DIR ?= $(ROOT_DIR)/build
INSTALL_DIR ?= $(ROOT_DIR)/install
LIB_DIR ?= $(INSTALL_DIR)/lib
BIN_DIR ?= $(INSTALL_DIR)/bin

TRACER_DIR ?= .
INCLUDE_DIR := $(TRACER_DIR)/include
SRC_DIR := $(TRACER_DIR)/src
GENERIC_SRC_DIR := $(TRACER_DIR)/src/generic
ARCH_SRC_DIR := $(TRACER_DIR)/src/$(ARCH)

GENERIC_SRCS := $(shell find $(GENERIC_SRC_DIR) -name \*.cpp)
ARCH_SRCS := $(shell find $(ARCH_SRC_DIR) -name \*.cpp)
SYSCALL_NAME_TABLE_SRC := $(SRC_DIR)/generated/syscall_names_table.cpp
ALL_SRCS := $(GENERIC_SRCS) $(ARCH_SRCS) $(SYSCALL_NAME_TABLE_SRC)
DEPENDENCIES := $(ALL_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.d)

GENERIC_OBJS := $(GENERIC_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
ARCH_OBJS := $(ARCH_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
SYSCALL_NAME_TABLE_OBJ := $(SYSCALL_NAME_TABLE_SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

CXX := g++
CXXFLAGS := -shared -std=c++11 -g -I$(INCLUDE_DIR) -Wall -Werror -fpic
LDFLAGS := -shared -g -L$(LIB_DIR) -Wl,-rpath=$(LIB_DIR)

.PHONY: all
all: $(LIB_DIR)/libtracer.so examples

$(SYSCALL_NAME_TABLE_SRC):
	mkdir -p $(@D)
	TRACER_DIR="$(TRACER_DIR)" $(TRACER_DIR)/build_scripts/build_syscall_names_table.rb "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(@D)
	$(CXX) -MMD -c $(CXXFLAGS) -o $@ $<

$(LIB_DIR)/libtracer.so: $(GENERIC_OBJS) $(ARCH_OBJS) $(SYSCALL_NAME_TABLE_OBJ)
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^

.PHONY: examples
examples:
	ROOT_DIR="$(ROOT_DIR)" TRACER_DIR="." make -f examples/Makefile

.PHONY: clean
clean:
	rm $(SYSCALL_NAME_TABLE_SRC)
	rm -r $(BUILD_DIR) $(INSTALL_DIR) || true

-include $(DEPENDENCIES)
