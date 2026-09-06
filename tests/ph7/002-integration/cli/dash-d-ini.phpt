--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter -d/-c php.ini directives
--SKIPIF--
<?php
// Spawns the interpreter through popen() with single-quoted `-r '...'` arguments —
// POSIX shell quoting that cmd.exe does not honour (php fails this on Windows too).
// The -d/-c behaviour itself is platform-neutral; only this harness is POSIX-shell.
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX shell quoting in the subprocess harness'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
function dashd_run($cmd) {
    $fp = popen($cmd, 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    return trim($out);
}
// -d name=value reaches both the INI view and the live knob
echo dashd_run("\"$phl\" -d session.name=CLISESS -r 'echo ini_get(\"session.name\"), \"|\", session_name(), \"|\", get_cfg_var(\"session.name\");'"), "\n";
// -d bare name defaults to "1"
echo dashd_run("\"$phl\" -d display_errors -r 'echo ini_get(\"display_errors\");'"), "\n";
// -c file, then -d overrides it
$ini = tempnam(sys_get_temp_dir(), 'phlini');
file_put_contents($ini, "[PHP]\n; comment\nsession.name = \"FILESESS\"\nmemory_limit = 128M\n");
echo dashd_run("\"$phl\" -c \"$ini\" -r 'echo ini_get(\"session.name\"), \"|\", ini_get(\"memory_limit\");'"), "\n";
echo dashd_run("\"$phl\" -c \"$ini\" -d session.name=DOVERRIDE -r 'echo ini_get(\"session.name\");'"), "\n";
// engine knob: -d error_reporting=0 silences the report flag
echo dashd_run("\"$phl\" -d error_reporting=0 -r 'echo error_reporting();'"), "\n";
@unlink($ini);
?>
--EXPECT--
CLISESS|CLISESS|CLISESS
1
FILESESS|128M
DOVERRIDE
0
--CLEAN--
<?php
unset($phl, $ini);
