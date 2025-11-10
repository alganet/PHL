@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: Script to download and install OpenCppCoverage

set "RELEASE_VERSION=0.9.9.0"
set "INSTALLER=OpenCppCoverageSetup-x64-%RELEASE_VERSION%.exe"
set "URL=https://github.com/OpenCppCoverage/OpenCppCoverage/releases/download/release-%RELEASE_VERSION%/%INSTALLER%"

if exist "%INSTALLER%" (
    echo Installer already exists, skipping download.
) else (
    echo Downloading OpenCppCoverage installer...
    powershell -Command "Invoke-WebRequest -Uri '%URL%' -OutFile '%INSTALLER%'"

    if %errorlevel% neq 0 (
        echo Failed to download the installer.
        exit /b 1
    )
)

echo Installing OpenCppCoverage...
%INSTALLER% /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-

if %errorlevel% neq 0 (
    echo Installation failed.
    exit /b 1
)

echo OpenCppCoverage installed successfully.