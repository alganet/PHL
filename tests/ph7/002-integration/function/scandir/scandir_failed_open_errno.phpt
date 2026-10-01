--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
scandir() reports a failed open twice: the opener's warning and the errno it left
--DESCRIPTION--
php's scandir() body raises its own line after the directory opener's warning --
'scandir(): (errno 2): No such file or directory' -- from the errno the failed open
left behind. opendir() and dir() say it once; this engine said it once for all
three.

On Windows php raises one more warning ahead of the opener's, the Win32 reason
with its code -- 'The system cannot find the file specifi (code: 2)', cut short
by php itself. The handler counts those instead of printing them, and the count
is pinned per platform.
--FILE--
<?php
$codes = 0;
set_error_handler(function ($no, $msg) use (&$dir, &$codes) {
    if (!(error_reporting() & $no)) { return false; }
    if (preg_match('/ \(code: 2\)$/', $msg)) { $codes++; return true; }
    echo "W: ", str_replace($dir, "<dir>", $msg), "\n"; return true;
});
$dir = sys_get_temp_dir() . '/phl_scandir_errno_' . getmypid();
echo "--- scandir\n";
var_dump(scandir($dir));
echo "--- opendir says it once\n";
var_dump(opendir($dir));
echo "--- dir() too\n";
var_dump(dir($dir));
echo "--- suppressed\n";
var_dump(@scandir($dir));
var_dump($codes === (PHP_OS_FAMILY === 'Windows' ? 3 : 0));
echo "done\n";
--EXPECT--
--- scandir
W: scandir(<dir>): Failed to open directory: No such file or directory
W: scandir(): (errno 2): No such file or directory
bool(false)
--- opendir says it once
W: opendir(<dir>): Failed to open directory: No such file or directory
bool(false)
--- dir() too
W: dir(<dir>): Failed to open directory: No such file or directory
bool(false)
--- suppressed
bool(false)
bool(true)
done
