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
	$(BUILD_DIR)/src/sx/sxmutex$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxstr$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxmem$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxds$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxutils$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxlib$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxfmt$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxxml$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxzip$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxrand$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/sx/sxhash$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/api$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/compile$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/constant$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/hashmap$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/lex$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/memobj$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/oo$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/parse$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/phl$(OBJ_SUFFIX)


# Test and Coverage Logic
TEST_PHL_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "./$(PHL_BIN)" \
	--target-dir tests
TEST_PHP_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHP_BIN)" \
	--target-dir tests
COVERAGE_PHL_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "./$(COVERAGE_BIN)" \
	--target-dir tests

# --- POLYGLOT MAGIC BEGINS
# \
!ifndef 0 # \
!include "build-aux/nmake.mk" # \
!include "$(MAKEDIR)/build-aux/patterns.mk" # \
!else
include build-aux/gnu.mk
include build-aux/rules.mk
# \
!endif
# --- POLYGLOT MAGIC ENDS

all: .ALWAYS $(PHL_BIN)
build: .ALWAYS $(PHL_BIN)
clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test
test-compat: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-compat
coverage: .ALWAYS $(PHL_BIN) $(COVERAGE_BIN) $(BUILD_DIR)/coverage/coverage.info
coverage-html: .ALWAYS $(PHL_BIN) $(COVERAGE_BIN) $(BUILD_DIR)/coverage/html
.ALWAYS:

$(BUILD_DIR)-test: $(PHL_BIN)
	@"$(PHL_BIN)" --version
	$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat: $(PHL_BIN)
	@"$(PHP_BIN)" --version
	@$(TEST_PHP_CMD) --output-format dot
	@"$(PHL_BIN)" --version
	@$(TEST_PHL_CMD) --output-format dot
