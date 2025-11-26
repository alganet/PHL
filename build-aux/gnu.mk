# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file MUST support GNU make 3.82

# Platform abstraction
OBJ_SUFFIX = .o
BIN_SUFFIX =

CC ?= cc
TARGET ?= $(shell CC=$(CC) ./build-aux/get_target.sh)
CFLAGS = -W -Wunused -Wall -Isrc/ph7 -Ofast $(PH7_DEFINES) -D__UNIXES__
LDFLAGS = -lm -lpthread
COVERAGE_CFLAGS = -W -Wunused -Wall -Isrc/ph7 -O0 $(PH7_DEFINES) -D__UNIXES__ -fprofile-arcs -ftest-coverage

PHP_BIN ?= $(shell command -v php)$(BIN_SUFFIX)

$(PHL_BIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(BUILD_DIR)-clean:
	-@rm -rf $(BUILD_DIR)

# COVERAGE
# --------

COVERAGE_LDFLAGS = $(LDFLAGS)
COVERAGE_OBJECTS = $(patsubst $(BUILD_DIR)/src/%,$(BUILD_DIR)/coverage/src/%,$(OBJECTS:.o=.o))

$(COVERAGE_BIN): $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) -o $@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(COVERAGE_BIN)
	@$(COVERAGE_PHL_CMD)
	@lcov --capture --rc geninfo_unexecuted_blocks=1 --quiet \
		--ignore-errors unsupported,unsupported \
		--include 'src/ph7/*' --directory $(BUILD_DIR) \
		--output-file $(BUILD_DIR)/coverage/coverage.info
	@sed 's|SF:.*/src/|SF:src/|g' \
		$(BUILD_DIR)/coverage/coverage.info > $(BUILD_DIR)/coverage/coverage.info.tmp \
			&& mv -f $(BUILD_DIR)/coverage/coverage.info.tmp $(BUILD_DIR)/coverage/coverage.info
	@$(COVERAGE_BIN) ./build-aux/lcov_info_to_text.php $(BUILD_DIR)/coverage/coverage.info

$(BUILD_DIR)/coverage/html: .ALWAYS $(BUILD_DIR)/coverage/coverage.info
	@genhtml \
		--include 'src/ph7/*' $(BUILD_DIR)/coverage/coverage.info \
		--output-directory $(BUILD_DIR)/coverage/html
	@echo "Coverage report generated in $(BUILD_DIR)/coverage/html/index.html"

