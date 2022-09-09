ROOT_DIR ?= ..
BUILD_DIR ?= $(ROOT_DIR)/build/examples
INSTALL_DIR ?= $(ROOT_DIR)/install

LIB_DIR := $(INSTALL_DIR)/lib
BIN_DIR := $(INSTALL_DIR)/bin

TRACER_DIR ?= ..
SRC_DIR := $(TRACER_DIR)/examples
INCLUDE_DIR := $(TRACER_DIR)/include

BINS := strace hello_world  #$(shell find $(SRC_DIR) -maxdepth 1 -type d)

CXX := g++
CXXFLAGS := -std=c++11 -g -I$(INCLUDE_DIR) -Wall -Werror
LDFLAGS := -std=c++11 -L$(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS := -ltracer

TARGETS := $(BINS:%=$(BIN_DIR)/%)

.PHONY: all
all: $(TARGETS)

srcs = $(wildcard $(SRC_DIR)/$(1)/*.cpp)
objs = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(call srcs,$(1)))
ident = $(1)

.SECONDEXPANSION:
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $^

$(TARGETS): $(BIN_DIR)/%: $$(call objs,%)
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
