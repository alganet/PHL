# SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

BUILD_DIR = build/$(shell $(CC) -dumpmachine)
CC ?= cc
CFLAGS = -W -Wunused -Wall -Isrc/ph7 -Ofast
LDFLAGS = -lm
PROGRAM = phl

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

all: $(BUILD_DIR)/$(PROGRAM)

# Pattern rules for compilation
$(BUILD_DIR)/src/ph7/%.o: src/ph7/%.c | $(BUILD_DIR)/src/ph7
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/phl/%.o: src/phl/%.c | $(BUILD_DIR)/src/phl
	$(CC) $(CFLAGS) -c $< -o $@

# Build target
$(BUILD_DIR)/$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

# Directory creation
$(BUILD_DIR)/src/ph7 $(BUILD_DIR)/src/phl:
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)/*

test: $(BUILD_DIR)/$(PROGRAM)
	@sh tests/phpt.sh \
          --target-executable $(BUILD_DIR)/phl \
          --target-dir vendor/php-src/tests

# Coverage build with gcov flags
COVERAGE_CFLAGS = $(CFLAGS) -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = $(LDFLAGS) -lgcov
COVERAGE_OBJECTS = $(OBJECTS:.o=.gcov.o)
COVERAGE_PROGRAM = $(BUILD_DIR)/phl-coverage

$(BUILD_DIR)/src/ph7/%.gcov.o: src/ph7/%.c | $(BUILD_DIR)/src/ph7
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/examples/%.gcov.o: examples/%.c | $(BUILD_DIR)/examples
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

$(COVERAGE_PROGRAM): $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) -o $@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

coverage: clean $(COVERAGE_PROGRAM)
	-@sh tests/phpt.sh \
          --target-executable $(COVERAGE_PROGRAM) \
          --target-dir vendor/php-src/tests
	@lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info
	@lcov --remove $(BUILD_DIR)/coverage.info '*/examples/*' --output-file $(BUILD_DIR)/coverage.info
	@genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage-html
	@echo "Coverage report generated in $(BUILD_DIR)/coverage-html/index.html"
