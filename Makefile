# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# Common definitions
default: full
BUILD_DIR = build/$(TARGET)
PH7_DEFINES = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS
FULL_PHL_BIN = $(BUILD_DIR)/full/phl$(BIN_SUFFIX)
COVERAGE_PHL_BIN = $(BUILD_DIR)/coverage/phl$(BIN_SUFFIX)

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
TEST_SMOKE_PHL_CMD = "$(FULL_PHL_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke
TEST_SMOKE_PHP_CMD = "$(PHP_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke
COVERAGE_SMOKE_PHL_CMD = "$(COVERAGE_PHL_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke

TEST_INTEGRATION_PHL_CMD = "$(FULL_PHL_BIN)" "tests/phpt.php" \
	--target-executable "./$(FULL_PHL_BIN)" \
	--target-dir tests/ph7/002-integration
TEST_INTEGRATION_PHP_CMD = "$(FULL_PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHP_BIN)" \
	--target-dir tests/ph7/002-integration
COVERAGE_INTEGRATION_PHL_CMD = "$(FULL_PHL_BIN)" "tests/phpt.php" \
	--target-executable "./$(COVERAGE_PHL_BIN)" \
	--target-dir tests/ph7/002-integration

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

full: .ALWAYS $(FULL_PHL_BIN)
clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS test-smoke test-integration
test-compat: .ALWAYS test-smoke-compat test-integration-compat
test-smoke: .ALWAYS $(FULL_PHL_BIN) $(BUILD_DIR)-test-smoke
test-smoke-compat: .ALWAYS $(FULL_PHL_BIN) $(BUILD_DIR)-test-smoke-compat
test-integration: .ALWAYS $(FULL_PHL_BIN) $(BUILD_DIR)-test-integration
test-integration-compat: .ALWAYS $(FULL_PHL_BIN) $(BUILD_DIR)-test-integration-compat
coverage: .ALWAYS $(FULL_PHL_BIN) $(COVERAGE_PHL_BIN) $(BUILD_DIR)/coverage/coverage.info
coverage-md: .ALWAYS $(FULL_PHL_BIN) $(COVERAGE_PHL_BIN) $(BUILD_DIR)/coverage/markdown
.ALWAYS:

$(BUILD_DIR)-test-smoke: $(FULL_PHL_BIN)
	@"$(FULL_PHL_BIN)" --version
	$(TEST_SMOKE_PHL_CMD) --output-format dot

$(BUILD_DIR)-test-smoke-compat: $(FULL_PHL_BIN)
	@"$(PHP_BIN)" --version
	@$(TEST_SMOKE_PHP_CMD) --output-format dot
	@"$(FULL_PHL_BIN)" --version
	@$(TEST_SMOKE_PHL_CMD) --output-format dot

$(BUILD_DIR)-test-integration: $(FULL_PHL_BIN)
	@"$(FULL_PHL_BIN)" --version
	$(TEST_INTEGRATION_PHL_CMD) --output-format dot

$(BUILD_DIR)-test-integration-compat: $(FULL_PHL_BIN)
	@"$(PHP_BIN)" --version
	@$(TEST_INTEGRATION_PHP_CMD) --output-format dot
	@"$(FULL_PHL_BIN)" --version
	@$(TEST_INTEGRATION_PHL_CMD) --output-format dot

$(BUILD_DIR)/coverage/markdown:
	@"$(FULL_PHL_BIN)" \
		"build-aux/lcov_to_markdown.php" \
		--output-dir="$(BUILD_DIR)/coverage/markdown" \
		"$(BUILD_DIR)/coverage/coverage.info"

