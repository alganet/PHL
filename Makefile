# SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

CC ?= cc
TARGET ?= $(shell CC=$(CC) ./build-aux/get_target.sh)

BUILD_DIR = build/$(TARGET)
CFLAGS = -W -Wunused -Wall -Isrc/ph7 -Ofast
LDFLAGS = -lm

.PHONY: all clean test test-compat coverage coverage-html
all: $(BUILD_DIR)/phl
clean: $(BUILD_DIR)-clean
test: $(BUILD_DIR)-test
test-compat: $(BUILD_DIR)-test-compat
coverage: $(BUILD_DIR)/coverage
coverage-html: $(BUILD_DIR)/coverage-html

# Source file lists
SRC_SOURCES = \
  api.c \
  builtin.c \
  compile.c \
  constant.c \
  hashmap.c \
  lex.c \
  lib.c \
  memobj.c \
  oo.c \
  parse.c \
  vfs.c \
  vm.c

# Object files
OBJECTS = \
  $(BUILD_DIR)/src/ph7/api.o \
  $(BUILD_DIR)/src/ph7/builtin.o \
  $(BUILD_DIR)/src/ph7/compile.o \
  $(BUILD_DIR)/src/ph7/constant.o \
  $(BUILD_DIR)/src/ph7/hashmap.o \
  $(BUILD_DIR)/src/ph7/lex.o \
  $(BUILD_DIR)/src/ph7/lib.o \
  $(BUILD_DIR)/src/ph7/memobj.o \
  $(BUILD_DIR)/src/ph7/oo.o \
  $(BUILD_DIR)/src/ph7/parse.o \
  $(BUILD_DIR)/src/ph7/vfs.o \
  $(BUILD_DIR)/src/ph7/vm.o \
  $(BUILD_DIR)/src/phl/phl.o

all: $(BUILD_DIR)/phl

# Pattern rules for compilation
$(BUILD_DIR)/src/ph7/%.o: src/ph7/%.c | $(BUILD_DIR)/src/ph7
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/phl/%.o: src/phl/%.c | $(BUILD_DIR)/src/phl
	$(CC) $(CFLAGS) -c $< -o $@

# Build target
$(BUILD_DIR)/phl: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

# Directory creation
$(BUILD_DIR)/src/ph7 $(BUILD_DIR)/src/phl:
	@mkdir -p $@

$(BUILD_DIR)-clean:
	@rm -rf $(BUILD_DIR)/*

# TESTING AND COVERAGE TARGETS
# ----------------------------

TEST_EXECUTABLE ?= $(BUILD_DIR)/phl
PHP_EXECUTABLE ?= $(shell command -v php)

TEST_PHL_CMD = $(BUILD_DIR)/phl -x tests/phpt.php \
	--target-executable $(TEST_EXECUTABLE) \
	--target-dir tests
TEST_PHP_CMD = $(BUILD_DIR)/phl -x tests/phpt.php \
	--target-executable $(PHP_EXECUTABLE) \
	--target-dir tests

$(BUILD_DIR)-test:
	@$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat:
	$(eval TEST_EXECUTABLE := $(BUILD_DIR)/phl)
	@$(TEST_EXECUTABLE) --version
	@$(TEST_PHL_CMD) --output-format dot
	@$(PHP_EXECUTABLE) --version
	@$(TEST_PHP_CMD) --output-format dot

COVERAGE_CFLAGS = $(CFLAGS) -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = $(LDFLAGS) -lgcov
COVERAGE_OBJECTS = $(OBJECTS:.o=.gcov.o)
COVERAGE_PHL_CMD = $(BUILD_DIR)/phl -x tests/phpt.php \
	--target-executable $(BUILD_DIR)/phl-coverage \
	--target-dir tests \
	--output-format dot

$(BUILD_DIR)/src/ph7/%.gcov.o: src/ph7/%.c | $(BUILD_DIR)/src/ph7
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/phl/%.gcov.o: src/phl/%.c | $(BUILD_DIR)/src/phl
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/phl-coverage: $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) -o $@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage.info: clean $(BUILD_DIR)/phl-coverage
	@$(COVERAGE_PHL_CMD)
	@lcov --capture --directory $(BUILD_DIR) --include 'src/*' --output-file $(BUILD_DIR)/coverage.info
	@lcov --list $(BUILD_DIR)/coverage.info
	@lcov --summary $(BUILD_DIR)/coverage.info

$(BUILD_DIR)/coverage-html: $(BUILD_DIR)/coverage.info
	@genhtml --include 'src/*' $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage-html
	@echo "Coverage report generated in $(BUILD_DIR)/coverage-html/index.html"

