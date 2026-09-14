@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: Script to install vcpkg and PCRE2 for local Windows development

set "VCPKG_DIR=%~dp0\..\vcpkg"
set "VCPKG_ZIP=vcpkg-master.zip"
set "URL=https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip"

if exist "%VCPKG_DIR%\vcpkg.exe" (
    echo vcpkg already installed in: %VCPKG_DIR%
    goto :install_packages
)

if exist "%VCPKG_ZIP%" (
    echo vcpkg zip already exists, skipping download.
) else (
    echo Downloading vcpkg...
    powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%VCPKG_ZIP%'"

    if %errorlevel% neq 0 (
        echo Failed to download vcpkg.
        exit /b 1
    )
)

echo Extracting vcpkg...
tar -xf "%VCPKG_ZIP%" -C "%~dp0\.."

if %errorlevel% neq 0 (
    echo Failed to extract vcpkg.
    exit /b 1
)

:: tar creates vcpkg-master/, rename to vcpkg/
if exist "%VCPKG_DIR%" rd /s /q "%VCPKG_DIR%"
move "%~dp0\..\vcpkg-master" "%VCPKG_DIR%"

echo Bootstrapping vcpkg...
call "%VCPKG_DIR%\bootstrap-vcpkg.bat" -disableMetrics

if %errorlevel% neq 0 (
    echo Failed to bootstrap vcpkg.
    exit /b 1
)

:install_packages
echo Installing pcre2:x64-windows-static...
"%VCPKG_DIR%\vcpkg.exe" install pcre2:x64-windows-static

if %errorlevel% neq 0 (
    echo Failed to install pcre2.
    exit /b 1
)

:: libxml2 backs the DOM / XMLWriter / libxml surface. The [core] feature set
:: drops the optional iconv/lzma/zlib transitive deps so the static link line
:: stays to just libxml2.lib (mirrors the POSIX pkg-config link).
echo Installing libxml2[core]:x64-windows-static...
"%VCPKG_DIR%\vcpkg.exe" install "libxml2[core]:x64-windows-static"

if %errorlevel% neq 0 (
    echo Failed to install libxml2.
    exit /b 1
)

echo.
echo vcpkg, PCRE2 and libxml2 installed successfully.
echo vcpkg root: %VCPKG_DIR%
