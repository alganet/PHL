# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# --- Mode system (defaults to "full") ---
# Usage: make MODE=full | make MODE=tiny | make MODE=coverage
MODE = full

# --- Mode definitions ---
# Each mode declares: <mode>_DEFINES  <mode>_EXTRA_CFLAGS (optional)
full_DEFINES      = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS -DPH7_ENABLE_NET -DPHL_ENABLE_SERVER -DPH7_ENABLE_PCRE
full_EXTRA_CFLAGS = $(PCRE2_CFLAGS)

tiny_DEFINES      = -DPH7_OMIT_FLOATING_POINT -DPH7_DISABLE_HASH_FUNC -DPH7_DISABLE_BUILTIN_FUNC -DPH7_DISABLE_DISK_IO
tiny_EXTRA_CFLAGS =

coverage_DEFINES      = -DPH7_ENABLE_MATH_FUNC -DPH7_ENABLE_THREADS -DPH7_ENABLE_NET -DPHL_ENABLE_SERVER -DPH7_ENABLE_PCRE
coverage_EXTRA_CFLAGS = $(PCRE2_CFLAGS)

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
	$(BUILD_DIR)/src/sx/sxblowfish$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/api$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin_date$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin_math$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/builtin_mb$(OBJ_SUFFIX) \
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
	$(BUILD_DIR)/src/ph7/vm_builtin_ini$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_ob$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_reflection$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_session$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_builtin_spl$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_http$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_http_response$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_json$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_pcre$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_serialize$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/vm_xml$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/ph7/net$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/phl$(OBJ_SUFFIX) \
	$(BUILD_DIR)/src/phl/server$(OBJ_SUFFIX)


# --- Test commands ---
# Smoke tests omit --target-executable so they run in-process (fast). That is
# safe only because the smoke corpus never calls exit/die or pollutes the
# interpreter; tests that need process isolation live in 002-integration and
# pass --target-executable below.
TEST_SMOKE_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke \
	--output-format dot
TEST_SMOKE_PHP_CMD = "$(PHP_BIN)" "tests/phpt.php" \
	--target-dir tests/ph7/001-smoke \
	--output-format dot
# Only integration is shardable ($(SHARD_FLAG)): each test runs in an isolated
# child process, so splitting the set across runners is safe. Smoke runs
# in-process in one shared interpreter (curated to pass only in the full sorted
# order), so sharding it would regroup tests and expose cross-test state leaks.
TEST_INTEGRATION_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHL_BIN)" \
	--target-dir tests/ph7/002-integration \
	--output-format dot $(SHARD_FLAG)
TEST_INTEGRATION_PHP_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHP_BIN)" \
	--target-dir tests/ph7/002-integration \
	--output-format dot

# Stress / fault-injection tests (opt-in; NOT part of `make test`). These run in
# child-process mode under a small per-allocation cap (PHL_MAX_ALLOC) so a
# deliberately oversized allocation deterministically triggers the out-of-memory
# paths. The cap is per-request (not cumulative), so the runner and the small
# tests run normally while only the oversized allocations fail. Long-term these
# migrate to per-test `--INI-- memory_limit=...` once php.ini lands.
TEST_STRESS_CMD = PHL_MAX_ALLOC=1048576 PHL_MAX_INPUT=32768 "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHL_BIN)" \
	--target-dir tests/ph7/003-stress \
	--output-format dot

# Deep-recursion tier (BYTECODE.md stage 3+5): PHP call depth is heap-bound and
# UNBOUNDED by default since stage 5, so these run at the stock host defaults —
# no PHL_MAX_RECURSION needed. Still WITHOUT the small per-allocation cap (deep
# frames legitimately grow single slot-table allocations past 1 MB).
TEST_DEEP_CMD = "$(PHL_BIN)" "tests/phpt.php" \
	--target-executable "$(PHL_BIN)" \
	--target-dir tests/ph7/004-deep \
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
test-stress: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-stress
test-smoke: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-smoke
test-smoke-compat: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-smoke-compat
test-integration: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-integration
test-integration-compat: .ALWAYS $(PHL_BIN) $(BUILD_DIR)-test-integration-compat
coverage-report: .ALWAYS $(PHL_BIN) $(BUILD_DIR)/coverage/coverage.info
coverage-md: .ALWAYS $(PHL_BIN) $(BUILD_DIR)/coverage/markdown
.ALWAYS:

# MODE=coverage is a build kind: the test targets gather coverage data by
# running the instrumented build under $(COVERAGE_RUN_*) (empty in normal builds;
# an OpenCppCoverage wrapper on MSVC). coverage-report/coverage-md only arrange
# the gathered data.
$(BUILD_DIR)-test-smoke: $(PHL_BIN)
	"$(PHL_BIN)" --version
	$(COVERAGE_RUN_SMOKE) $(TEST_SMOKE_CMD)

$(BUILD_DIR)-test-smoke-compat: $(PHL_BIN)
	"$(PHP_BIN)" --version
	$(TEST_SMOKE_PHP_CMD)
	"$(PHL_BIN)" --version
	$(TEST_SMOKE_CMD)

$(BUILD_DIR)-test-integration: $(PHL_BIN)
	"$(PHL_BIN)" --version
	$(COVERAGE_RUN_INTEGRATION) $(TEST_INTEGRATION_CMD)

# Opt-in stress/fault-injection suite (POSIX shell; sets PHL_MAX_ALLOC inline).
$(BUILD_DIR)-test-stress: $(PHL_BIN)
	"$(PHL_BIN)" --version
	$(TEST_STRESS_CMD)
	$(TEST_DEEP_CMD)

$(BUILD_DIR)-test-integration-compat: $(PHL_BIN)
	"$(PHP_BIN)" --version
	$(TEST_INTEGRATION_PHP_CMD)
	"$(PHL_BIN)" --version
	$(TEST_INTEGRATION_CMD)

# Oracle-coverage gap report (POSIX shell): lists every test that SKIPS under
# exactly one engine — the §6 oracle-blind debt as a tracked, only-goes-down
# number. Runs smoke + integration in TAP mode under both engines and compares
# the skip sets; the skip REASONS printed by the runner say why each side skips.
test-oracle-gap: .ALWAYS $(PHL_BIN)
	@mkdir -p "$(BUILD_DIR)/oracle-gap"
	@echo "# collecting smoke skips (phl in-process / php oracle)..."
	@"$(PHL_BIN)" tests/phpt.php --target-dir tests/ph7/001-smoke \
		| grep '^ok .* # skip' | awk '{print $$4}' | sort > "$(BUILD_DIR)/oracle-gap/smoke-phl.txt" || true
	@"$(PHP_BIN)" tests/phpt.php --target-dir tests/ph7/001-smoke \
		| grep '^ok .* # skip' | awk '{print $$4}' | sort > "$(BUILD_DIR)/oracle-gap/smoke-php.txt" || true
	@echo "# collecting integration skips (phl target / php target)..."
	@"$(PHL_BIN)" tests/phpt.php --target-executable "$(PHL_BIN)" --target-dir tests/ph7/002-integration \
		| grep '^ok .* # skip' | awk '{print $$4}' | sort > "$(BUILD_DIR)/oracle-gap/integ-phl.txt" || true
	@"$(PHL_BIN)" tests/phpt.php --target-executable "$(PHP_BIN)" --target-dir tests/ph7/002-integration \
		| grep '^ok .* # skip' | awk '{print $$4}' | sort > "$(BUILD_DIR)/oracle-gap/integ-php.txt" || true
	@echo ""
	@echo "# ============ ORACLE GAP REPORT ============"
	@echo "# smoke: skip under php only (oracle-blind):"
	@comm -13 "$(BUILD_DIR)/oracle-gap/smoke-phl.txt" "$(BUILD_DIR)/oracle-gap/smoke-php.txt" | sed 's/^/#   /'
	@echo "# smoke: skip under phl only:"
	@comm -23 "$(BUILD_DIR)/oracle-gap/smoke-phl.txt" "$(BUILD_DIR)/oracle-gap/smoke-php.txt" | sed 's/^/#   /'
	@echo "# integration: skip under php target only (oracle-blind):"
	@comm -13 "$(BUILD_DIR)/oracle-gap/integ-phl.txt" "$(BUILD_DIR)/oracle-gap/integ-php.txt" | sed 's/^/#   /'
	@echo "# integration: skip under phl target only:"
	@comm -23 "$(BUILD_DIR)/oracle-gap/integ-phl.txt" "$(BUILD_DIR)/oracle-gap/integ-php.txt" | sed 's/^/#   /'
	@echo "# ------------------------------------------"
	@printf '# oracle-blind totals: smoke %s + integration %s = ' \
		"$$(comm -13 '$(BUILD_DIR)/oracle-gap/smoke-phl.txt' '$(BUILD_DIR)/oracle-gap/smoke-php.txt' | wc -l | tr -d ' ')" \
		"$$(comm -13 '$(BUILD_DIR)/oracle-gap/integ-phl.txt' '$(BUILD_DIR)/oracle-gap/integ-php.txt' | wc -l | tr -d ' ')"
	@echo "$$(( $$(comm -13 '$(BUILD_DIR)/oracle-gap/smoke-phl.txt' '$(BUILD_DIR)/oracle-gap/smoke-php.txt' | wc -l) + $$(comm -13 '$(BUILD_DIR)/oracle-gap/integ-phl.txt' '$(BUILD_DIR)/oracle-gap/integ-php.txt' | wc -l) )) tests never run under the php oracle"

$(BUILD_DIR)/coverage/markdown:
	@"$(PHL_BIN)" \
		"build-aux/lcov_to_markdown.php" \
		--output-dir="$(BUILD_DIR)/coverage/markdown" \
		"$(BUILD_DIR)/coverage/coverage.info"
