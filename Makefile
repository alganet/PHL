# SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

CC ?= cc
TARGET ?= $(shell CC=$(CC) ./build-aux/get_target.sh)

BUILD_DIR = build/$(TARGET)
CFLAGS = -W -Wunused -Wall -Isrc/ph7 -Ofast -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS -D__UNIXES__
LDFLAGS = -lm -lpthread

all: .ALWAYS $(BUILD_DIR)/phl
build: .ALWAYS $(BUILD_DIR)/phl
clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS $(BUILD_DIR)-test
test-compat: .ALWAYS $(BUILD_DIR)-test-compat
coverage: .ALWAYS $(BUILD_DIR)/coverage/coverage.info
coverage-html: .ALWAYS $(BUILD_DIR)/coverage/html
.ALWAYS: # No .PHONY to keep consistent with nmake.mk

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

# Pattern rules for compilation
$(BUILD_DIR)/src/ph7/%.o: src/ph7/%.c | $(BUILD_DIR)/src/ph7
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/src/phl/%.o: src/phl/%.c | $(BUILD_DIR)/src/phl
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/src/ph7 $(BUILD_DIR)/src/phl:
	@mkdir -p $@
$(BUILD_DIR)/phl: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(BUILD_DIR)-clean:
	@rm -rf $(BUILD_DIR)/*

# TEST TARGETS
# ------------

TEST_EXECUTABLE ?= $(BUILD_DIR)/phl
PHP_EXECUTABLE ?= $(shell command -v php)

TEST_PHL_CMD = $(TEST_EXECUTABLE) tests/phpt.php \
	--target-dir tests
TEST_PHP_CMD = $(PHP_EXECUTABLE) tests/phpt.php \
	--target-dir tests

$(BUILD_DIR)-test: $(BUILD_DIR)/phl
	@$(TEST_EXECUTABLE) --version
	$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat: $(BUILD_DIR)/phl
	@$(TEST_EXECUTABLE) --version
	@$(TEST_PHL_CMD) --output-format dot
	@$(PHP_EXECUTABLE) --version
	@$(TEST_PHP_CMD) --output-format dot

# COVERAGE TARGETS
# ----------------

COVERAGE_CFLAGS = $(filter-out -Ofast, $(CFLAGS)) -O0 -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = $(LDFLAGS) -lgcov
COVERAGE_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/coverage/%,$(OBJECTS:.o=.gcov.o))

COVERAGE_PHL_CMD = $(BUILD_DIR)/coverage/phl-coverage tests/phpt.php \
	--target-dir tests \
	--output-format dot

$(BUILD_DIR)/coverage/ph7/%.gcov.o: src/ph7/%.c | $(BUILD_DIR)/coverage/ph7
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@
$(BUILD_DIR)/coverage/phl/%.gcov.o: src/phl/%.c | $(BUILD_DIR)/coverage/phl
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@
$(BUILD_DIR)/coverage/ph7 $(BUILD_DIR)/coverage/phl:
	@mkdir -p $@
$(BUILD_DIR)/coverage/phl-coverage: $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) -o $@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(BUILD_DIR)/coverage/phl-coverage
	@$(COVERAGE_PHL_CMD)
	@lcov --capture --rc geninfo_unexecuted_blocks=1 --quiet \
		--include 'src/ph7/*' --directory $(BUILD_DIR) \
		--output-file $(BUILD_DIR)/coverage/coverage.info
	@sed -i 's|SF:$(CURDIR)/|SF:|g' $(BUILD_DIR)/coverage/coverage.info
	@$(BUILD_DIR)/coverage/phl-coverage ./build-aux/lcov_info_to_text.php $(BUILD_DIR)/coverage/coverage.info

$(BUILD_DIR)/coverage/html: .ALWAYS $(BUILD_DIR)/coverage/coverage.info
	@genhtml \
		--include 'src/ph7/*' $(BUILD_DIR)/coverage/coverage.info \
		--output-directory $(BUILD_DIR)/coverage/html
	@echo "Coverage report generated in $(BUILD_DIR)/coverage/html/index.html"

