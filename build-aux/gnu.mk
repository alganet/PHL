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

# Per-mode optimization and instrumentation
full_OPT_CFLAGS     = -O3 -ffast-math
tiny_OPT_CFLAGS     = -Oz
coverage_OPT_CFLAGS = -O0 -fprofile-arcs -ftest-coverage

full_LDFLAGS = -lm -lpthread
tiny_LDFLAGS =
coverage_LDFLAGS = -lm -lpthread -fprofile-arcs -ftest-coverage

PH7_DEFINES = $($(MODE)_DEFINES)
MODE_EXTRA_CFLAGS = $($(MODE)_EXTRA_CFLAGS)
MODE_CFLAGS = $(BASE_CFLAGS) $($(MODE)_OPT_CFLAGS) $(PH7_DEFINES) $(MODE_EXTRA_CFLAGS)
MODE_LDFLAGS = $($(MODE)_LDFLAGS)

PHP_BIN ?= $(shell command -v php)$(BIN_SUFFIX)

MODE_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/$(MODE)/src/%,$(OBJECTS))

$(PHL_BIN): $(MODE_OBJECTS)
	$(CC) $(MODE_CFLAGS) -o $@ $(MODE_OBJECTS) $(MODE_LDFLAGS)

$(BUILD_DIR)-clean:
	-@rm -rf $(BUILD_DIR)

# COVERAGE
# --------

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(PHL_BIN)
	@$(TEST_SMOKE_CMD)
	@$(TEST_INTEGRATION_CMD)
	@lcov --capture --rc geninfo_unexecuted_blocks=1 --quiet \
		--ignore-errors unsupported,unsupported \
		--include 'src/ph7/*' --include 'src/sx/*' --include 'src/phl/*' --directory $(BUILD_DIR) \
		--output-file $(BUILD_DIR)/coverage/coverage.info
	@sed 's|SF:.*/src/|SF:src/|g' \
		$(BUILD_DIR)/coverage/coverage.info > $(BUILD_DIR)/coverage/coverage.info.tmp \
			&& mv -f $(BUILD_DIR)/coverage/coverage.info.tmp $(BUILD_DIR)/coverage/coverage.info
	@$(PHL_BIN) ./build-aux/lcov_info_to_text.php $(BUILD_DIR)/coverage/coverage.info
