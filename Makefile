# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# --- Mode system (defaults to "full") ---
# Usage: make MODE=full | make MODE=tiny | make MODE=coverage
MODE = full

# --- Mode definitions ---
# Each mode declares: <mode>_DEFINES  <mode>_EXTRA_CFLAGS (optional)
full_DEFINES      = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS -DPH7_ENABLE_NET -DPHL_ENABLE_SERVER
full_EXTRA_CFLAGS =

tiny_DEFINES      = -DPH7_OMIT_FLOATING_POINT -DPH7_DISABLE_HASH_FUNC -DPH7_DISABLE_BUILTIN_FUNC -DPH7_DISABLE_DISK_IO
tiny_EXTRA_CFLAGS =

coverage_DEFINES      = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS -DPH7_ENABLE_NET -DPHL_ENABLE_SERVER
coverage_EXTRA_CFLAGS =

# --- Derived variables ---
BUILD_DIR = build/$(TARGET)
PHL_BIN = $(BUILD_DIR)/$(MODE)/phl$(BIN_SUFFIX)
TOOL_BIN = $(BUILD_DIR)/full/phl$(BIN_SUFFIX)

# Object files (mode-neutral paths, rewritten per-mode by platform includes)
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
	$(BUILD_DIR)/src/ph7/builtin_date$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin_math$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/compile$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/constant$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/hashmap$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/lex$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/memobj$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/oo$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/parse$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs_unix$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs_win$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vfs_zip$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_class$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_getopt$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_ob$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_http$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_json$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_xml$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/net$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/phl$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/server$(OBJ_SUFFIX)


# --- Test commands ---
TEST_SMOKE_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke \
	--output-format dot
TEST_SMOKE_PHP_CMD = "$(PHP_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke \
	--output-format dot

TEST_INTEGRATION_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHL_BIN)" \
	--target-dir tests/ph7/002-integration \
	--output-format dot
TEST_INTEGRATION_PHP_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHP_BIN)" \
	--target-dir tests/ph7/002-integration \
	--output-format dot

default: build

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

build: .ALWAYS $(PHL_BIN)
	"$(PHL_BIN)" --version

clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS test-smoke test-integration
test-compat: .ALWAYS test-smoke-compat test-integration-compat
test-smoke: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-smoke
test-smoke-compat: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-smoke-compat
test-integration: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-integration
test-integration-compat: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-integration-compat
coverage-report: .ALWAYS $(PHL_BIN) $(BUILD_DIR)/coverage/coverage.info
coverage-md: .ALWAYS $(PHL_BIN) $(BUILD_DIR)/coverage/markdown
.ALWAYS:

$(BUILD_DIR)-test-smoke: $(PHL_BIN)
	"$(PHL_BIN)" --version
	$(TEST_SMOKE_CMD) 

$(BUILD_DIR)-test-smoke-compat: $(PHL_BIN)
	"$(PHP_BIN)" --version
	$(TEST_SMOKE_PHP_CMD)
	"$(PHL_BIN)" --version
	$(TEST_SMOKE_CMD)

$(BUILD_DIR)-test-integration: $(PHL_BIN)
	"$(PHL_BIN)" --version
	$(TEST_INTEGRATION_CMD) 

$(BUILD_DIR)-test-integration-compat: $(PHL_BIN)
	"$(PHP_BIN)" --version
	$(TEST_INTEGRATION_PHP_CMD) 
	"$(PHL_BIN)" --version
	$(TEST_INTEGRATION_CMD) 

$(BUILD_DIR)/coverage/markdown:
	@"$(PHL_BIN)" \
		"build-aux/lcov_to_markdown.php" \
		--output-dir="$(BUILD_DIR)/coverage/markdown" \
		"$(BUILD_DIR)/coverage/coverage.info"
