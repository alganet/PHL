# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

# Platform abstraction
OBJ_SUFFIX = .o
BIN_SUFFIX =

CC ?= cc
TARGET ?= $(shell CC=$(CC) ./build-aux/get_target.sh)

# Base flags shared by all modes
BASE_CFLAGS = -W -Wunused -Wall -Werror -Isrc/sx -Isrc/ph7 -D__UNIXES__

# PCRE2 detection via pkg-config (empty when absent or for tiny mode)
PCRE2_CFLAGS := $(shell pkg-config --cflags libpcre2-8 2>/dev/null)
PCRE2_LIBS   := $(shell pkg-config --libs   libpcre2-8 2>/dev/null)

# Per-mode optimization and instrumentation
full_OPT_CFLAGS     = -O3
tiny_OPT_CFLAGS     = -Oz
coverage_OPT_CFLAGS = -O0 -fprofile-arcs -ftest-coverage

full_LDFLAGS = -lm -lpthread $(PCRE2_LIBS)
tiny_LDFLAGS =
coverage_LDFLAGS = -lm -lpthread $(PCRE2_LIBS) -fprofile-arcs -ftest-coverage

PH7_DEFINES = $($(MODE)_DEFINES)
MODE_EXTRA_CFLAGS = $($(MODE)_EXTRA_CFLAGS)
MODE_CFLAGS = $(BASE_CFLAGS) $($(MODE)_OPT_CFLAGS) $(PH7_DEFINES) $(MODE_EXTRA_CFLAGS)
MODE_LDFLAGS = $($(MODE)_LDFLAGS)

PHP_BIN ?= $(shell command -v php)$(BIN_SUFFIX)

# Optional test-shard selector (e.g. SHARD=2/4); empty = run all tests.
# Lets CI fan a slow run (coverage) across parallel workers.
SHARD ?=
SHARD_FLAG = $(if $(SHARD),--shard $(SHARD))

# Coverage collection wrappers for the test recipes. gcov instruments at compile
# time, so running the MODE=coverage build just emits .gcda -- no wrapper needed.
COVERAGE_RUN_SMOKE =
COVERAGE_RUN_INTEGRATION =

MODE_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/$(MODE)/src/%,$(OBJECTS))

$(PHL_BIN): $(MODE_OBJECTS)
	$(CC) $(MODE_CFLAGS) -o $@ $(MODE_OBJECTS) $(MODE_LDFLAGS)

$(BUILD_DIR)-clean:
	-@rm -rf $(BUILD_DIR)

# COVERAGE
# --------
# coverage-report only *arranges* data already gathered by `make MODE=coverage
# test*` (which emits .gcda). lcov captures whatever .gcda exist -- a full run or
# a single shard.

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(PHL_BIN)
	@lcov --capture --rc geninfo_unexecuted_blocks=1 --quiet \
		--ignore-errors unsupported,unsupported \
		--include 'src/ph7/*' --include 'src/sx/*' --include 'src/phl/*' --directory $(BUILD_DIR) \
		--output-file $(BUILD_DIR)/coverage/coverage.info
	@sed 's|SF:.*/src/|SF:src/|g' \
		$(BUILD_DIR)/coverage/coverage.info > $(BUILD_DIR)/coverage/coverage.info.tmp \
			&& mv -f $(BUILD_DIR)/coverage/coverage.info.tmp $(BUILD_DIR)/coverage/coverage.info
	@$(PHL_BIN) ./build-aux/lcov_info_to_text.php $(BUILD_DIR)/coverage/coverage.info
