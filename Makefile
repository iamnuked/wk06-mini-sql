CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Iinclude
BUILD_DIR := build

COMMON_SOURCES := \
	src/common.c \
	src/ast.c \
	src/lexer.c \
	src/parser.c \
	src/storage.c \
	src/executor.c

APP_SOURCES := $(COMMON_SOURCES) src/main.c
TEST_SOURCES := $(COMMON_SOURCES) tests/test_runner.c

.PHONY: all clean test demo

all: $(BUILD_DIR)/mini_sql $(BUILD_DIR)/test_runner

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/mini_sql: $(APP_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(APP_SOURCES) -o $@

$(BUILD_DIR)/test_runner: $(TEST_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_SOURCES) -o $@

test: all
	./$(BUILD_DIR)/test_runner

demo: $(BUILD_DIR)/mini_sql
	./$(BUILD_DIR)/mini_sql examples/db examples/sql/demo_workflow.sql

clean:
	rm -rf $(BUILD_DIR)
