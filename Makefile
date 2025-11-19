# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# Common definitions
default: all
BUILD_DIR = build/$(TARGET)
PH7_DEFINES = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS
PHL_BIN = $(BUILD_DIR)/phl$(BIN_SUFFIX)
COVERAGE_BIN = $(BUILD_DIR)/coverage/phl-coverage$(BIN_SUFFIX)

# Object files
OBJECTS = \
	$(BUILD_DIR)/src/ph7/api$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/compile$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/constant$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/hashmap$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/lex$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/lib$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/memobj$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/oo$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/parse$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/phl$(OBJ_SUFFIX)


# Test and Coverage Logic
TEST_PHL_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-dir tests
TEST_PHP_CMD = "$(PHP_BIN)" "tests/phpt.php" \
	--target-dir tests
COVERAGE_PHL_CMD = "$(COVERAGE_BIN)" "tests/phpt.php" \
	--target-dir tests \
	--output-format dot

# --- POLYGLOT MAGIC BEGINS
# \
!ifndef 0 # \
!include "build-aux/nmake.mk" # \
!else
include build-aux/gnu.mk
# \
!endif
# --- POLYGLOT MAGIC ENDS

all: .ALWAYS $(PHL_BIN)
build: .ALWAYS $(PHL_BIN)
clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS $(BUILD_DIR)-test
test-compat: .ALWAYS $(BUILD_DIR)-test-compat
coverage: .ALWAYS $(BUILD_DIR)/coverage/coverage.info
coverage-html: .ALWAYS $(BUILD_DIR)/coverage/html
.ALWAYS:

$(BUILD_DIR)-test: $(PHL_BIN)
	@"$(PHL_BIN)" --version
	$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat: $(PHL_BIN)
	@"$(PHL_BIN)" --version
	@$(TEST_PHL_CMD) --output-format dot
	@"$(PHP_BIN)" --version
	@$(TEST_PHP_CMD) --output-format dot
