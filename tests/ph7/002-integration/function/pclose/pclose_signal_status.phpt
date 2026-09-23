--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pclose() reports a signal death as the signal number, the way php does
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    echo "skip a signal death is a POSIX wait-status shape; cmd.exe has no equivalent, and the ordinary exit codes are covered by function/pclose/pclose.phpt on both platforms";
}
?>
--FILE--
<?php
/* php translates ONE case of waitpid()'s status word -- a normal exit becomes
 * its exit code -- and answers the raw word otherwise, so a process killed by a
 * signal reports the SIGNAL. PHL used to add the shell's own 128 to it (143 for
 * SIGTERM), which is a number an ordinary `exit 143` also produces. */
foreach (['kill -TERM $$', 'kill -9 $$', 'exit 42', 'true'] as $cmd) {
    $fp = popen($cmd, 'r');
    printf("%-15s %d\n", $cmd, pclose($fp));
}
?>
--EXPECT--
kill -TERM $$   15
kill -9 $$      9
exit 42         42
true            0
