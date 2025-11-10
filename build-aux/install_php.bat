@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: Script to download and install the latest Windows PHP release

set "PHP_VERSION=8.4.14"
set "PHP_ZIP=php-%PHP_VERSION%-nts-Win32-vs17-x64.zip"
set "PHP_DIR=%~dp0\..\php"
set "URL=https://windows.php.net/downloads/releases/%PHP_ZIP%"

if exist "%PHP_DIR%\php.exe" (
    echo PHP is already installed in: %PHP_DIR%
    exit /b 0
)

if exist "%PHP_ZIP%" (
    echo PHP zip already exists, skipping download.
) else (
    echo Downloading PHP %PHP_VERSION%...
    powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%PHP_ZIP%'"

    if %errorlevel% neq 0 (
        echo Failed to download PHP.
        exit /b 1
    )
)

echo Extracting PHP...
powershell -Command "Expand-Archive -Path '%PHP_ZIP%' -DestinationPath '%PHP_DIR%' -Force"

if %errorlevel% neq 0 (
    echo Failed to extract PHP.
    exit /b 1
)

move "%PHP_DIR%\php-win.exe" "%PHP_DIR%\php.exe"

echo PHP installed successfully in: %PHP_DIR%
echo PHP executable: %PHP_DIR%\php.exe