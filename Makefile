CXX := g++

DUMP_DIR := dumps
CXXFLAGS := -Iinclude \
            -fsanitize=address,undefined,leak \
            -fno-omit-frame-pointer \
            -Wshadow -Winit-self -Wredundant-decls \
            -Wcast-align -Wundef -Wfloat-equal -Winline \
            -Wunreachable-code -Wmissing-declarations \
            -Wmissing-include-dirs -Wswitch-enum -Wswitch-default \
            -Weffc++ -Wmain -Wextra -Wall -g -pipe \
            -fexceptions -Wcast-qual -Wconversion \
            -Wctor-dtor-privacy -Wempty-body -Wformat-security \
            -Wformat=2 -Wignored-qualifiers -Wlogical-op \
            -Wno-missing-field-initializers -Wnon-virtual-dtor \
            -Woverloaded-virtual -Wpointer-arith -Wsign-promo \
            -Wstack-usage=8192 -Wstrict-aliasing \
            -Wstrict-null-sentinel -Wtype-limits \
            -Wwrite-strings -Werror=vla \
            -D_DEBUG -D_EJUDGE_CLIENT_SIDE #-DHASH_MAP_LOGGER_ALL

LDFLAGS := -fsanitize=address,undefined,leak

SRC_DIR := source

BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/test

SOURCES := $(wildcard $(SRC_DIR)/*.cpp) $(STACK_DIR)/stack.cpp $(STACK_DIR)/void_stack.cpp $(LIST_DIR)/source/list_operations.cpp $(LIST_DIR)/source/list_verification.cpp
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp)) \

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)  \
	rm -rf *.html                   \
    rm -rf *.pdf                    \
    rm -rf *.toc                    \
    rm -rf *.log                    \
    rm -rf *.out                    \
    rm -rf *.aux                    \
    rm -rf *.fls                    \
    rm -rf *.gz                     \
    rm -rf *.fdb_latexmk            \
    rm -rf graphs
	rm -rf $(DUMP_DIR)
