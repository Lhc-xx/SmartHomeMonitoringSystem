CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -pedantic

BUILD_DIR := build
TEST_TARGET := $(BUILD_DIR)/server_test

.PHONY: all test run clean

all: test

test: $(TEST_TARGET)
	$(TEST_TARGET)

run: $(TEST_TARGET)
	$(TEST_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_TARGET): $(BUILD_DIR) tests/Test.cc tests/test_main.cc
	$(CXX) $(CXXFLAGS) tests/Test.cc tests/test_main.cc -o $(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)
