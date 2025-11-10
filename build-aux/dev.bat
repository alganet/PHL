@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: NMake wrapper for raw cmd.exe
:: ---
::
:: Usage: dev.bat [options]
:: Example: .\win\dev.bat nmake /nologo /f win\nmake.mk

cd /d %~dp0

set "dev_cmd=%~n0.cmd"

:: Reuse cached dev.cmd if it exists
if exist "%~dp0%dev_cmd%" (
    call "%~dp0%dev_cmd%" %*
    exit /b %errorlevel%
)

:: Calculate next year for checking future VS versions
for /f %%a in ('powershell -NoProfile -Command "(Get-Date).Year"') do set year=%%a
set /a next_year=%year% + 1

:: Check if nmake.exe is in PATH
set "vcvars_path="
where nmake >nul 2>nul
if errorlevel 1 (
    :: Search for vcvarsall.bat in Visual Studio installations
    for %%P in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
        for /L %%V in (2015,1,%next_year%) do (
            for %%E in ("BuildTools" "Community" "Professional" "Enterprise") do (
                if exist "%%~P\Microsoft Visual Studio\%%V\%%~E\VC\Auxiliary\Build\vcvarsall.bat" (
                    set "vcvars_path=%%~P\Microsoft Visual Studio\%%V\%%~E\VC\Auxiliary\Build\vcvarsall.bat"
                )
            )
        )
    )
    if not defined vcvars_path (
        echo Error: Visual Studio not found.
        echo Please install Visual Studio with C++ build tools and the Windows SDK.
        exit /b 1
    )
)

:: Check if OpenCppCoverage is in the PATH
set "opencppcoverage_path="
where OpenCppCoverage >nul 2>nul
if errorlevel 1 (
    :: Search for OpenCppCoverage in common installation paths
    for %%P in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
        if exist "%%~P\OpenCppCoverage\OpenCppCoverage.exe" (
            set "opencppcoverage_path=%%~P\OpenCppCoverage"
        )
    )
    if not defined opencppcoverage_path (
        echo Warning: OpenCppCoverage not found. Will not run coverage tests.
        echo To install it, run build-aux\install_opencppcoverage.bat.
    )
)

:: Check if php is in the PATH
where php >nul 2>nul
if errorlevel 1 (
    :: Search for ..\php installation
    if exist "%~dp0..\php\php-win.exe" (
        set "php_path=%~dp0..\php"
    ) else (
        echo Warning: PHP not found. Will not run compatibility tests.
        echo To install it, run build-aux\install_php.bat.
    )
)

:: Create cached dev.cmd with detected vcvarsall.bat path
echo @echo off > %dev_cmd%
echo :: This file is auto-generated. Do not edit. >> %dev_cmd%
if defined vcvars_path (
    echo call "%vcvars_path%" x64 ^>nul >> %dev_cmd%
)
if defined php_path (
    echo set "PATH=%%PATH%%;%php_path%" >> %dev_cmd%
)
if defined opencppcoverage_path (
    echo set "PATH=%%PATH%%;%opencppcoverage_path%" >> %dev_cmd%
)
echo cd .. >> %dev_cmd%
echo %%* >> %dev_cmd%
:: Execute the build
call %dev_cmd% %*
exit /b %errorlevel%
