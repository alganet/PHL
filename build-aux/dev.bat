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

:: Reuse cached dev.cmd if it exists.
::
:: NOT written as `if exist (...)`: cmd expands %errorlevel% when it PARSES a
:: parenthesised block, before anything in the block runs, so `exit /b
:: %errorlevel%` there reports the status from BEFORE the call -- 0 -- and this
:: branch exited GREEN over a failed build. A bare `exit /b` is not the fix
:: either; it sets 0 rather than preserving the errorlevel. Read outside any
:: block, on its own line, it is expanded when that line executes.
::
:: CI never saw this: dev.cmd is gitignored and each job runs dev.bat once on a
:: fresh runner, so the detection branch below -- whose `exit /b %errorlevel%`
:: was already outside any block -- is the one that runs there. What did see it
:: is every second build on one tree, which is what dev.sh and any local
:: rebuild do.
if not exist "%~dp0%dev_cmd%" goto :no_cached_cmd
call "%~dp0%dev_cmd%" %*
exit /b %errorlevel%
:no_cached_cmd

:: Locate vcvarsall.bat. Try in order:
::   (1) nmake already in PATH (developer prompt)
::   (2) vswhere.exe — Microsoft's official VS locator, ships with every
::       VS Installer; works for all layouts (year-based "2022" or
::       version-based "18", BuildTools-only, Preview, etc.)
::   (3) Hardcoded directory walk as a last resort. Covers both the
::       legacy year-based (Microsoft Visual Studio\2017..\) and the
::       new version-based (Microsoft Visual Studio\17, 18, ...) schemes
::       Microsoft introduced with VS 2026.
set "vcvars_path="
where nmake.exe >nul 2>nul
if not errorlevel 1 goto :have_vc

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" set "vswhere=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" set "vcvars_path=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

if not defined vcvars_path (
    for %%P in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
        for %%V in ("2017" "2019" "2022" "2025" "2026" "15" "16" "17" "18" "19") do (
            for %%E in ("BuildTools" "Community" "Professional" "Enterprise" "Preview") do (
                if exist "%%~P\Microsoft Visual Studio\%%~V\%%~E\VC\Auxiliary\Build\vcvarsall.bat" (
                    set "vcvars_path=%%~P\Microsoft Visual Studio\%%~V\%%~E\VC\Auxiliary\Build\vcvarsall.bat"
                )
            )
        )
    )
)

if not defined vcvars_path (
    echo Error: Visual Studio not found.
    echo Searched:
    echo   nmake.exe in PATH
    echo   vswhere.exe at "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    echo   "%%ProgramFiles%%\Microsoft Visual Studio\^(year or major-version^)\^<edition^>\VC\Auxiliary\Build\vcvarsall.bat"
    echo Please install Visual Studio with C++ build tools and the Windows SDK.
    exit /b 1
)
:have_vc

:: Check if OpenCppCoverage is in the PATH
set "opencppcoverage_path="
where OpenCppCoverage.exe >nul 2>nul
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

:: Check if vcpkg is available (for PCRE2)
set "vcpkg_root="
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        set "vcpkg_root=%VCPKG_ROOT%"
    )
)
if not defined vcpkg_root (
    if exist "%~dp0..\vcpkg\vcpkg.exe" (
        set "vcpkg_root=%~dp0..\vcpkg"
    )
)
if not defined vcpkg_root (
    where vcpkg.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%i in ('where vcpkg.exe') do set "vcpkg_root=%%~dpi"
    )
)
if not defined vcpkg_root (
    echo Warning: vcpkg not found. PCRE functions will not be available.
    echo To install it, run build-aux\install_vcpkg.bat.
)

:: Check if php is in the PATH
where php.exe >nul 2>nul
if errorlevel 1 (
    :: Search for ..\php installation
    if exist "%~dp0..\php\php.exe" (
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
if defined vcpkg_root (
    echo set "VCPKG_ROOT=%vcpkg_root%" >> %dev_cmd%
)
echo cd .. >> %dev_cmd%
echo %%* >> %dev_cmd%
:: Execute the build
call %dev_cmd% %*
exit /b %errorlevel%
