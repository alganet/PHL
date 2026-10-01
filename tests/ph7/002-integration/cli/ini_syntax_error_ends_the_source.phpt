--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini: a syntax error stops the rest of ITS source, and only that source
--SKIPIF--
<?php
// Spawns the interpreter through popen() with single-quoted `-r '...'` arguments —
// POSIX shell quoting that cmd.exe does not honour (php fails this on Windows too).
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX shell quoting in the subprocess harness'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$ini = tempnam(sys_get_temp_dir(), 'phlini');
function ini_run($cmd, $ini) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    // a diagnostic names the file with its symlinks resolved (macOS /private)
    return str_replace([realpath($ini), $ini], 'INI', trim($out));
}
$show = 'echo ini_get("precision"), "/", ini_get("serialize_precision");';
// A -c file is ONE parse: the directive under a bad value is never seen, so
// serialize_precision keeps what the line ABOVE the error gave it, not 13.
file_put_contents($ini, "serialize_precision=11\nprecision=1 & )\nserialize_precision=13\n");
echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
// The same file without the bad line runs to the end.
file_put_contents($ini, "serialize_precision=11\nprecision=7\nserialize_precision=13\n");
echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
// A committed value is still an error when a byte is left over, and it stops
// the source just the same: precision holds 1, serialize_precision holds 11.
file_put_contents($ini, "serialize_precision=11\nprecision=1)\nserialize_precision=13\n");
echo ini_run("\"$phl\" -c \"$ini\" -r '$show'", $ini), "\n";
// The -d options are their own parse. A bad php.ini does not take them down...
file_put_contents($ini, "serialize_precision=11\nprecision=1 & )\nserialize_precision=13\n");
echo ini_run("\"$phl\" -c \"$ini\" -d precision=9 -r '$show'", $ini), "\n";
// ...and a bad -d drops only the -d behind it, leaving the file's lines standing.
file_put_contents($ini, "serialize_precision=11\nprecision=7\n");
echo ini_run("\"$phl\" -c \"$ini\" -d \"serialize_precision=1 & )\" -d precision=9 -r '$show'", $ini), "\n";
@unlink($ini);
?>
--EXPECT--
PHP:  syntax error, unexpected ')' in INI on line 2
14/11
7/13
PHP:  syntax error, unexpected ')' in INI on line 2
1/11
PHP:  syntax error, unexpected ')' in INI on line 2
9/11
PHP:  syntax error, unexpected ')' in Unknown on line 6
7/11
--CLEAN--
<?php
unset($phl, $ini, $show);
