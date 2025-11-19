# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe

# Platform abstraction
OBJ_SUFFIX = .obj
BIN_SUFFIX = .exe

CC = cl
TARGET = x86_64-windows-msvc
CFLAGS = /nologo /Fd$(BUILD_DIR:/=\)\ph7.pdb /I src /I src/ph7 /W4 /Ox $(PH7_DEFINES)
LDFLAGS = /nologo /link advapi32.lib dbghelp.lib /subsystem:console /entry:mainCRTStartup
COVERAGE_CFLAGS = /nologo /Fd$(BUILD_DIR:/=\)\coverage\ph7-coverage.pdb /I src /I src/ph7 /W4 /Od /Zi $(PH7_DEFINES)

PHP_BIN = php$(BIN_SUFFIX)

# Pattern rules for compilation
{src/ph7}.c{$(BUILD_DIR)/src/ph7}.obj:
	@if not exist "$(BUILD_DIR)/src/ph7" mkdir "$(BUILD_DIR)/src/ph7"
	$(CC) $(CFLAGS) /Fo"$@" /c $<
{src/phl}.c{$(BUILD_DIR)/src/phl}.obj:
	@if not exist "$(BUILD_DIR)/src/phl" mkdir "$(BUILD_DIR)/src/phl"
	$(CC) $(CFLAGS) /Fo"$@" /c $<
$(BUILD_DIR):
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
$(PHL_BIN): $(BUILD_DIR) $(OBJECTS)
	$(CC) $(CFLAGS) /Fe$@ $(OBJECTS) $(LDFLAGS)

# Clean target
$(BUILD_DIR)-clean:
	-@rd /s /q $(BUILD_DIR:/=\) 2>nul

# COVERAGE
# --------

COVERAGE_LDFLAGS = $(LDFLAGS) /DEBUG
COVERAGE_OBJECTS = $(OBJECTS:/src/=/coverage/)

{src/ph7}.c{$(BUILD_DIR)/coverage/ph7}.obj:
	@if not exist "$(BUILD_DIR)/coverage/ph7" mkdir "$(BUILD_DIR)/coverage/ph7"
	$(CC) $(COVERAGE_CFLAGS) /Fo"$@" /c $<
{src/phl}.c{$(BUILD_DIR)/coverage/phl}.obj:
	@if not exist "$(BUILD_DIR)/coverage/phl" mkdir "$(BUILD_DIR)/coverage/phl"
	$(CC) $(COVERAGE_CFLAGS) /Fo"$@" /c $<
$(BUILD_DIR)/coverage:
	@if not exist "$(BUILD_DIR)/coverage" mkdir "$(BUILD_DIR)/coverage"
$(COVERAGE_BIN): $(BUILD_DIR)/coverage $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) /Fe$@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS $(COVERAGE_BIN)
	-@OpenCppCoverage.exe --quiet \
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
