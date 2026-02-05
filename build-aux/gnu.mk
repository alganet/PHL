# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

# Platform abstraction
OBJ_SUFFIX = .o
BIN_SUFFIX =

CC ?= cc
TARGET ?= $(shell CC=$(CC) ./build-aux/get_target.sh)
full_CFLAGS = -W -Wunused -Wall -Isrc/sx -Isrc/ph7 -Ofast $(PH7_DEFINES) -D__UNIXES__
LDFLAGS = -lm -lpthread
coverage_CFLAGS = -W -Wunused -Wall -Isrc/sx -Isrc/ph7 -O0 $(PH7_DEFINES) -D__UNIXES__ -fprofile-arcs -ftest-coverage

PHP_BIN ?= $(shell command -v php)$(BIN_SUFFIX)

FULL_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/full/src/%,$(OBJECTS))

$(FULL_PHL_BIN): $(FULL_OBJECTS)
	$(CC) $(full_CFLAGS) -o $@ $(FULL_OBJECTS) $(LDFLAGS)

$(BUILD_DIR)-clean:
	-@rm -rf $(BUILD_DIR)

# COVERAGE
# --------

COVERAGE_LDFLAGS = $(LDFLAGS)
COVERAGE_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/coverage/src/%,$(OBJECTS))

$(COVERAGE_PHL_BIN): $(COVERAGE_OBJECTS)
	$(CC) $(coverage_CFLAGS) -o $@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(COVERAGE_PHL_BIN)
	@$(COVERAGE_SMOKE_PHL_CMD)
	@$(COVERAGE_INTEGRATION_PHL_CMD)
	@lcov --capture --rc geninfo_unexecuted_blocks=1 --quiet \
		--ignore-errors unsupported,unsupported \
		--include 'src/ph7/*' --include 'src/sx/*' --include 'src/phl/*' --directory $(BUILD_DIR) \
		--output-file $(BUILD_DIR)/coverage/coverage.info
	@sed 's|SF:.*/src/|SF:src/|g' \
		$(BUILD_DIR)/coverage/coverage.info > $(BUILD_DIR)/coverage/coverage.info.tmp \
			&& mv -f $(BUILD_DIR)/coverage/coverage.info.tmp $(BUILD_DIR)/coverage/coverage.info
	@$(FULL_PHL_BIN) ./build-aux/lcov_info_to_text.php $(BUILD_DIR)/coverage/coverage.info
