# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe

# Platform abstraction
OBJ_SUFFIX = .obj
BIN_SUFFIX = .exe

CC = cl
TARGET = x86_64-windows-msvc
CFLAGS = /nologo /Fd$(BUILD_DIR:/=\)\ph7.pdb /I src /I src/sx /I src/ph7 /W4 /Ox $(PH7_DEFINES:-=/)
LDFLAGS = /nologo /link advapi32.lib /subsystem:console /entry:mainCRTStartup
COVERAGE_CFLAGS = /nologo /Fd$(BUILD_DIR:/=\)\coverage\ph7-coverage.pdb /I src /I src/sx /I src/ph7 /W4 /Od /Zi $(PH7_DEFINES:-=/) /DPH7_DEBUG

PHP_BIN = php$(BIN_SUFFIX)

$(PHL_BIN): $(BUILD_DIR) $(OBJECTS)
	$(CC) $(CFLAGS) /Fe$@ $(OBJECTS) $(LDFLAGS)

$(BUILD_DIR)-clean:
	-@rd /s /q $(BUILD_DIR:/=\) 2>nul

# COVERAGE
# --------

COVERAGE_LDFLAGS = /nologo /link advapi32.lib dbghelp.lib /subsystem:console /entry:mainCRTStartup
COVERAGE_OBJECTS = $(OBJECTS:/src/=/coverage/src/)

$(COVERAGE_BIN): $(BUILD_DIR)/coverage $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) /Fe$@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(COVERAGE_BIN)
	@OpenCppCoverage.exe --quiet \
	--sources "$(MAKEDIR)\src" \
	--export_type cobertura:$(BUILD_DIR)/coverage/cobertura.xml \
	-- $(COVERAGE_PHL_CMD)
	@"$(COVERAGE_BIN)" \
		"build-aux/cobertura_xml_to_lcov_info.php" \
		"$(BUILD_DIR)/coverage/cobertura.xml" > "$(BUILD_DIR)/coverage/coverage.info"
	@"$(COVERAGE_BIN)" \
		"build-aux/lcov_info_to_text.php" \
		"$(BUILD_DIR)/coverage/coverage.info"

$(BUILD_DIR)/coverage/html: .ALWAYS $(COVERAGE_BIN)
	-@OpenCppCoverage.exe --quiet \
	--sources $(MAKEDIR)/src \
	--export_type html:$(BUILD_DIR)/coverage/html \
	-- $(COVERAGE_PHL_CMD)
