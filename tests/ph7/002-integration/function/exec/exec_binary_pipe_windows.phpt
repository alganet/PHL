--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
system()/passthru() hand back what the command WROTE on Windows, CRs included
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Windows') {
    echo "skip the CR is what a text-mode pipe eats, and only cmd.exe writes one; the POSIX side asserts passthru()'s byte-exactness in exec_lines_posix.phpt";
}
?>
--FILE--
<?php
/* php opens the pipe "rb" for these two (php_exec) and "rt" for shell_exec(), so
 * on Windows the runners hand back the command's bytes untouched while
 * shell_exec() gets the CRLF translated to LF. Asserted as hex on purpose: the
 * phpt runner normalises line endings in the OUTPUT, which would hide exactly
 * the byte this is about. */
ob_start();
$exb_r = system('echo s1& echo s2', $exb_code);
$exb_out = ob_get_clean();
var_dump(bin2hex($exb_out), $exb_r, $exb_code);

ob_start();
passthru('echo p1& echo p2');
$exb_raw = ob_get_clean();
var_dump(bin2hex($exb_raw));

/* php's own exception, and the reason this cannot be one rule for all four */
var_dump(bin2hex(shell_exec('echo se')));
?>
--EXPECT--
string(16) "73310d0a73320d0a"
string(2) "s2"
int(0)
string(16) "70310d0a70320d0a"
string(6) "73650a"
