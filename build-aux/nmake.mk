# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe

CC = cl
TARGET = x86_64-windows-msvc

BUILD_DIR = build\$(TARGET)
CFLAGS = /nologo /Fd$(BUILD_DIR)\ph7.pdb /I src /I src\ph7 /W4 /Ox /DPH7_ENABLE_MATH_FUNC /DPH7_ENABLE_THREADS
LDFLAGS = /nologo /link advapi32.lib dbghelp.lib /subsystem:console /entry:mainCRTStartup

all: .ALWAYS $(BUILD_DIR)\phl.exe
build: .ALWAYS $(BUILD_DIR)\phl.exe
clean: .ALWAYS $(BUILD_DIR)-clean
test: .ALWAYS $(BUILD_DIR)-test
test-compat: .ALWAYS $(BUILD_DIR)-test-compat
coverage: .ALWAYS $(BUILD_DIR)\coverage\coverage.info
coverage-html: .ALWAYS $(BUILD_DIR)\coverage\html
.ALWAYS: # No .PHONY because nmake does not support it

# Object files
OBJECTS = \
	$(BUILD_DIR)\src\ph7\api.obj \
	$(BUILD_DIR)\src\ph7\builtin.obj \
	$(BUILD_DIR)\src\ph7\compile.obj \
	$(BUILD_DIR)\src\ph7\constant.obj \
	$(BUILD_DIR)\src\ph7\hashmap.obj \
	$(BUILD_DIR)\src\ph7\lex.obj \
	$(BUILD_DIR)\src\ph7\lib.obj \
	$(BUILD_DIR)\src\ph7\memobj.obj \
	$(BUILD_DIR)\src\ph7\oo.obj \
	$(BUILD_DIR)\src\ph7\parse.obj \
	$(BUILD_DIR)\src\ph7\vfs.obj \
	$(BUILD_DIR)\src\ph7\vm.obj \
	$(BUILD_DIR)\src\phl\phl.obj

# Pattern rules for compilation
{src\ph7}.c{$(BUILD_DIR)\src\ph7}.obj:
	@if not exist $(BUILD_DIR)\src\ph7 mkdir $(BUILD_DIR)\src\ph7
	$(CC) $(CFLAGS) /Fo"$@" /c $<
{src\phl}.c{$(BUILD_DIR)\src\phl}.obj:
	@if not exist $(BUILD_DIR)\src\phl mkdir $(BUILD_DIR)\src\phl
	$(CC) $(CFLAGS) /Fo"$@" /c $<
$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
$(BUILD_DIR)\phl.exe: $(BUILD_DIR) $(OBJECTS)
	$(CC) $(CFLAGS) /Fe$@ $(OBJECTS) $(LDFLAGS)

# Clean target
$(BUILD_DIR)-clean:
	-@del /s /q $(BUILD_DIR)\* >nul

# TEST TARGETS
# ------------

TEST_EXECUTABLE = $(BUILD_DIR)\phl.exe
PHP_EXECUTABLE = php.exe

TEST_PHL_CMD = $(TEST_EXECUTABLE) tests/phpt.php \
	--target-dir tests
TEST_PHP_CMD = $(PHP_EXECUTABLE) tests/phpt.php \
	--target-dir tests

$(BUILD_DIR)-test: $(BUILD_DIR)\phl.exe
	@$(TEST_EXECUTABLE) --version
	$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat: $(BUILD_DIR)\phl.exe
	@$(TEST_EXECUTABLE) --version
	@$(TEST_PHL_CMD) --output-format dot
	@$(PHP_EXECUTABLE) --version
	@$(TEST_PHP_CMD) --output-format dot

# COVERAGE TARGETS
# ----------------

COVERAGE_CFLAGS = $(subst /Ox,/Od,$(CFLAGS)) /Fd$(BUILD_DIR)\coverage\ph7-coverage.pdb /Zi
COVERAGE_LDFLAGS = $(LDFLAGS) /DEBUG
COVERAGE_OBJECTS = $(OBJECTS:\src\=\coverage\)

COVERAGE_PHL_CMD = $(BUILD_DIR)\coverage\phl-coverage.exe tests/phpt.php \
	--target-dir tests \
	--output-format dot

{src\ph7}.c{$(BUILD_DIR)\coverage\ph7}.obj:
	@if not exist $(BUILD_DIR)\coverage\ph7 mkdir $(BUILD_DIR)\coverage\ph7
	$(CC) $(COVERAGE_CFLAGS) /Fo"$@" /c $<
{src\phl}.c{$(BUILD_DIR)\coverage\phl}.obj:
	@if not exist $(BUILD_DIR)\coverage\phl mkdir $(BUILD_DIR)\coverage\phl
	$(CC) $(COVERAGE_CFLAGS) /Fo"$@" /c $<
$(BUILD_DIR)\coverage:
	@if not exist $(BUILD_DIR)\coverage mkdir $(BUILD_DIR)\coverage
$(BUILD_DIR)\coverage\phl-coverage.exe: $(BUILD_DIR)\coverage $(COVERAGE_OBJECTS)
	$(CC) $(COVERAGE_CFLAGS) /Fe$@ $(COVERAGE_OBJECTS) $(COVERAGE_LDFLAGS)

$(BUILD_DIR)\coverage\coverage.info: .ALWAYS $(BUILD_DIR)\coverage\phl-coverage.exe
	-@OpenCppCoverage.exe --quiet \
	--sources $(MAKEDIR)\src \
	--export_type cobertura:$(BUILD_DIR)\coverage\cobertura.xml \
	-- $(COVERAGE_PHL_CMD)
	@$(BUILD_DIR)\coverage\phl-coverage.exe \
		build-aux/cobertura_xml_to_lcov_info.php \
		$(BUILD_DIR)\coverage\cobertura.xml > $(BUILD_DIR)\coverage\coverage.info
	@$(BUILD_DIR)\coverage\phl-coverage.exe \
		build-aux/lcov_info_to_text.php \
		$(BUILD_DIR)\coverage\coverage.info

$(BUILD_DIR)\coverage\html: .ALWAYS $(BUILD_DIR)\coverage\phl-coverage.exe
	-@OpenCppCoverage.exe --quiet \
	--sources $(MAKEDIR)\src \
	--export_type html:$(BUILD_DIR)\coverage\html \
	-- $(COVERAGE_PHL_CMD)
