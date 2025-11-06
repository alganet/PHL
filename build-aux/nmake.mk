# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe

BUILD_DIR = build\x86_64-windows-msvc
CC = cl
CFLAGS = /Fd$(BUILD_DIR)\ph7.pdb /I src /W4 /Ox
LDFLAGS = /link advapi32.lib /nologo /subsystem:console /entry:mainCRTStartup
PROGRAM = ph7.exe

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

EXAMPLE_SOURCES = ph7_interp.c

# Object files
OBJECTS = \
  $(BUILD_DIR)\src\api.obj \
  $(BUILD_DIR)\src\builtin.obj \
  $(BUILD_DIR)\src\compile.obj \
  $(BUILD_DIR)\src\constant.obj \
  $(BUILD_DIR)\src\hashmap.obj \
  $(BUILD_DIR)\src\lex.obj \
  $(BUILD_DIR)\src\lib.obj \
  $(BUILD_DIR)\src\memobj.obj \
  $(BUILD_DIR)\src\oo.obj \
  $(BUILD_DIR)\src\parse.obj \
  $(BUILD_DIR)\src\vfs.obj \
  $(BUILD_DIR)\src\vm.obj \
  $(BUILD_DIR)\examples\ph7_interp.obj

all: $(BUILD_DIR)\$(PROGRAM)

# Inference rules for compilation
{src}.c{$(BUILD_DIR)\src}.obj:
  @if not exist $(BUILD_DIR)\src mkdir $(BUILD_DIR)\src
  $(CC) $(CFLAGS) /Fo"$@" /c $<

{examples}.c{$(BUILD_DIR)\examples}.obj:
  @if not exist $(BUILD_DIR)\examples mkdir $(BUILD_DIR)\examples
  $(CC) $(CFLAGS) /Fo"$@" /c $<

# Build target
$(BUILD_DIR)\$(PROGRAM): $(BUILD_DIR) $(OBJECTS)
  $(CC) $(CFLAGS) /Fe$@ $(OBJECTS) $(LDFLAGS)

# Directory creation
$(BUILD_DIR):
  @if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

clean:
  @del /s /q $(BUILD_DIR)\*
