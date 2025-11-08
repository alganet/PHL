# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe

CC = cl
TARGET = x86_64-windows-msvc

BUILD_DIR = build\$(TARGET)
CFLAGS = /Fd$(BUILD_DIR)\ph7.pdb /I src\ph7 /W4 /Ox /DPH7_ENABLE_MATH_FUNC
LDFLAGS = /link advapi32.lib /nologo /subsystem:console /entry:mainCRTStartup

all: $(BUILD_DIR)\phl.exe
clean: $(BUILD_DIR)-clean
test: $(BUILD_DIR)-test
test-compat: $(BUILD_DIR)-test-compat

# Source file lists
SRC_SOURCES = \
  api.c \
  builtin.c \
  compile.c \
  constant.c \
  hashmap.c \
  lex.c \
  lib.c \
  memobj.c \
  oo.c \
  parse.c \
  vfs.c \
  vm.c

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

# Inference rules for compilation
{src\ph7}.c{$(BUILD_DIR)\src\ph7}.obj:
  @if not exist $(BUILD_DIR)\src\ph7 mkdir $(BUILD_DIR)\src\ph7
  $(CC) $(CFLAGS) /Fo"$@" /c $<

{src\phl}.c{$(BUILD_DIR)\src\phl}.obj:
  @if not exist $(BUILD_DIR)\src\phl mkdir $(BUILD_DIR)\src\phl
  $(CC) $(CFLAGS) /Fo"$@" /c $<

# Build target
$(BUILD_DIR)\phl.exe: $(BUILD_DIR) $(OBJECTS)
  $(CC) $(CFLAGS) /Fe$@ $(OBJECTS) $(LDFLAGS)

# Directory creation
$(BUILD_DIR):
  @if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(BUILD_DIR)-clean:
  @del /s /q $(BUILD_DIR)\*

# TESTING AND COVERAGE TARGETS
# ----------------------------

TEST_EXECUTABLE = $(BUILD_DIR)\phl.exe -x
PHP_EXECUTABLE = php.exe

TEST_PHL_CMD = $(TEST_EXECUTABLE) tests/phpt.php \
	--target-dir tests
TEST_PHP_CMD = $(PHP_EXECUTABLE) tests/phpt.php \
	--target-dir tests

$(BUILD_DIR)-test:
	@$(TEST_PHL_CMD)

$(BUILD_DIR)-test-compat:
	@$(BUILD_DIR)\phl.exe --version
	@$(TEST_PHL_CMD) --output-format dot
	@$(PHP_EXECUTABLE) --version
	@$(TEST_PHP_CMD) --output-format dot
