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

:: The php.ini is (re)written on EVERY run, including the already-installed one:
:: it is this script's real product, it changes more often than the tarball, and a
:: VM installed by an older revision would otherwise keep an oracle configured the
:: wrong way forever. Writing it is cheap; downloading again is not.
if exist "%PHP_DIR%\php.exe" (
    echo PHP is already installed in: %PHP_DIR%
    call :write_ini
    "%PHP_DIR%\php.exe" --version
    exit /b 0
)

echo Resolving latest %PHP_BRANCH% NTS x64 build from windows.php.net...
:: TLS 1.2 is mandatory: Windows PowerShell 5.1 negotiates TLS 1.0/1.1 by default,
:: which windows.php.net's CDN refuses ("underlying connection was closed"), so the
:: download fails on a stock VM without it.
powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $ErrorActionPreference='Stop'; $b='%PHP_BRANCH%'; $dir='%PHP_DIR%'; $j=Invoke-RestMethod 'https://windows.php.net/downloads/releases/releases.json'; $v=$j.$b; if(-not $v){throw \"no releases for branch $b\"}; $p=$v.PSObject.Properties | Where-Object { $_.Name -match 'nts-vs\d+-x64' } | Select-Object -First 1; if(-not $p){throw \"no NTS x64 build for $b\"}; $zip=$p.Value.zip.path; $url='https://windows.php.net/downloads/releases/'+$zip; Write-Host \"Downloading $url\"; Invoke-WebRequest -Uri $url -OutFile $zip; if(Test-Path $dir){Remove-Item -Recurse -Force $dir}; Expand-Archive -Path $zip -DestinationPath $dir -Force; if(-not (Test-Path (Join-Path $dir 'php.exe'))){throw 'php.exe missing after extract'}"
if %errorlevel% neq 0 (
    echo Failed to install PHP.
    exit /b 1
)

call :write_ini

echo.
echo PHP oracle installed in: %PHP_DIR%
"%PHP_DIR%\php.exe" --version
endlocal
exit /b 0

:: ---------------------------------------------------------------------------
:: The oracle's configuration, pinned so test-compat is reproducible instead of
:: depending on ambient defaults: assertions off like php's production ini, and a
:: UTC clock to match CI.
::
:: The EXTENSIONS matter just as much, and the zip ships them unloaded. A Windows
:: php.exe has dom / libxml / SimpleXML / xmlreader / xmlwriter / tokenizer /
:: iconv / session built in, but mbstring is a DLL nobody enables -- so every
:: `mb_*` call in the corpus was `Call to undefined function` under the Windows
:: oracle, and the first one (function/mb/mb_detect_encoding.phpt) took the
:: in-process runner down with it, aborting the whole run at test ~1200. Nothing
:: else here needs enabling: the extensions PHL does not implement are out of
:: scope by policy, so no cross-engine test can reach them.
:: ---------------------------------------------------------------------------
:write_ini
> "%PHP_DIR%\php.ini" echo zend.assertions=-1
>>"%PHP_DIR%\php.ini" echo date.timezone=UTC
>>"%PHP_DIR%\php.ini" echo extension_dir="%PHP_DIR%\ext"
>>"%PHP_DIR%\php.ini" echo extension=mbstring
exit /b 0
