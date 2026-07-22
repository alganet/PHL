@echo off
:: SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
:: SPDX-License-Identifier: BSD-3-Clause

:: Download the Windows PHP build used as the cross-check oracle for test-compat.
:: The exact patch filename is resolved from windows.php.net's releases.json at run
:: time, so it never 404s when a release rotates from /releases/ into the archive.
:: Usage: install_php.bat [branch]   (branch defaults to 8.5 to match the corpus)

setlocal
set "PHP_BRANCH=%~1"
if "%PHP_BRANCH%"=="" set "PHP_BRANCH=8.5"
set "PHP_DIR=%~dp0..\php"

if exist "%PHP_DIR%\php.exe" (
    echo PHP is already installed in: %PHP_DIR%
    "%PHP_DIR%\php.exe" --version
    exit /b 0
)

echo Resolving latest %PHP_BRANCH% NTS x64 build from windows.php.net...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $b='%PHP_BRANCH%'; $dir='%PHP_DIR%'; $j=Invoke-RestMethod 'https://windows.php.net/downloads/releases/releases.json'; $v=$j.$b; if(-not $v){throw \"no releases for branch $b\"}; $p=$v.PSObject.Properties | Where-Object { $_.Name -match 'nts-vs\d+-x64' } | Select-Object -First 1; if(-not $p){throw \"no NTS x64 build for $b\"}; $zip=$p.Value.zip.path; $url='https://windows.php.net/downloads/releases/'+$zip; Write-Host \"Downloading $url\"; Invoke-WebRequest -Uri $url -OutFile $zip; if(Test-Path $dir){Remove-Item -Recurse -Force $dir}; Expand-Archive -Path $zip -DestinationPath $dir -Force; if(-not (Test-Path (Join-Path $dir 'php.exe'))){throw 'php.exe missing after extract'}"
if %errorlevel% neq 0 (
    echo Failed to install PHP.
    exit /b 1
)

echo.
echo PHP oracle installed in: %PHP_DIR%
"%PHP_DIR%\php.exe" --version
endlocal
