# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# This file must support nmake.exe
# Note: nmake does not support $($(VAR)) recursive expansion.
# Per-mode CFLAGS and link rules are generated in patterns.mk.

# Platform abstraction
OBJ_SUFFIX = .obj
BIN_SUFFIX = .exe

CC = cl
TARGET = x86_64-windows-msvc

# PCRE2 via vcpkg (set VCPKG_ROOT or use default GitHub Actions path)
!IF "$(VCPKG_ROOT)" != ""
VCPKG_INSTALLED = $(VCPKG_ROOT)\installed\x64-windows-static
!ELSE
VCPKG_INSTALLED = C:\vcpkg\installed\x64-windows-static
!ENDIF
PCRE2_CFLAGS = /I "$(VCPKG_INSTALLED)\include"
PCRE2_LIBS = "$(VCPKG_INSTALLED)\lib\pcre2-8.lib"

# Base flags shared by all modes
BASE_CFLAGS = /nologo /I src /I src/sx /I src/ph7 /W4 /WX

# Per-mode CFLAGS (used by patterns.mk generated rules)
full_CFLAGS = $(BASE_CFLAGS) /Ox $(full_DEFINES:-=/) $(full_EXTRA_CFLAGS) /Fd$(BUILD_DIR:/=\)\ph7.pdb
tiny_CFLAGS = $(BASE_CFLAGS) /Os $(tiny_DEFINES:-=/) $(tiny_EXTRA_CFLAGS) /Fd$(BUILD_DIR:/=\)\ph7.pdb
coverage_CFLAGS = $(BASE_CFLAGS) /Od /Zi $(coverage_DEFINES:-=/) $(coverage_EXTRA_CFLAGS) /DPH7_DEBUG /Fd$(BUILD_DIR:/=\)\coverage\ph7-coverage.pdb

# Per-mode LDFLAGS (used by patterns.mk generated link rules)
# bcrypt.lib provides BCryptGenRandom, used by SyOSCSPRNG in src/sx/sxrand.c.
full_LDFLAGS = /nologo /link advapi32.lib bcrypt.lib ws2_32.lib $(PCRE2_LIBS) /subsystem:console /entry:mainCRTStartup
tiny_LDFLAGS = /nologo /link advapi32.lib bcrypt.lib /subsystem:console /entry:mainCRTStartup
coverage_LDFLAGS = /nologo /link advapi32.lib bcrypt.lib dbghelp.lib ws2_32.lib $(PCRE2_LIBS) /subsystem:console /entry:mainCRTStartup

LDFLAGS = $(full_LDFLAGS)

PHP_BIN = php$(BIN_SUFFIX)

# NOTE: The $(PHL_BIN) rule and patterns are provided by patterns.mk
# to avoid complex IF selections and nested macro expansions in NMake.

$(BUILD_DIR)-clean:
	-@rd /s /q $(BUILD_DIR:/=\) 2>nul

# COVERAGE
# --------

$(BUILD_DIR)/coverage/coverage.info: .ALWAYS
	@OpenCppCoverage.exe --quiet \
	--sources "$(MAKEDIR)\src" \
	--excluded_sources "$(MAKEDIR)\src\minidump.h" \
	--cover_children \
	--export_type cobertura:$(BUILD_DIR)/coverage/smoke.xml \
	-- $(TEST_SMOKE_CMD)

	@OpenCppCoverage.exe --quiet \
	--sources "$(MAKEDIR)\src" \
	--excluded_sources "$(MAKEDIR)\src\minidump.h" \
	--cover_children \
	--export_type cobertura:$(BUILD_DIR)/coverage/integration.xml \
	-- $(TEST_INTEGRATION_CMD)
	@"$(PHL_BIN)" \
		"build-aux/cobertura_xml_to_lcov_info.php" \
		"$(BUILD_DIR)/coverage/smoke.xml" \
		"$(BUILD_DIR)/coverage/integration.xml" \
		> "$(BUILD_DIR)/coverage/coverage.info"
	@"$(PHL_BIN)" \
		"build-aux/lcov_info_to_text.php" \
		"$(BUILD_DIR)/coverage/coverage.info"
