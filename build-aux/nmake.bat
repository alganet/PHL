@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: NMake wrapper for raw cmd.exe
:: ---
::
:: Usage: nmake.bat [options]
:: Example: .\win\nmake.bat /nologo /f win\nmake.mk

cd /d %~dp0

set "nmake_cmd=%~n0.cmd"

:: Reuse cached nmake.cmd if it exists
if exist "%~dp0%nmake_cmd%" (
    call "%~dp0%nmake_cmd%" %*
    exit /b %errorlevel%
)

:: Calculate next year for checking future VS versions
for /f %%a in ('powershell -NoProfile -Command "(Get-Date).Year"') do set year=%%a
set /a next_year=%year% + 1

:: Check if cl.exe is in PATH
set "vcvars_path="
where cl >nul 2>nul
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
        exit /b 1
    )
)

:: Create cached nmake.cmd with detected vcvarsall.bat path
echo @echo off > %nmake_cmd%
echo :: This file is auto-generated. Do not edit. >> %nmake_cmd%
echo call "%vcvars_path%" x64 ^>nul >> %nmake_cmd%
echo cd .. >> %nmake_cmd%
echo nmake %%* >> %nmake_cmd%

:: Execute the build
call %nmake_cmd% %*
exit /b %errorlevel%
