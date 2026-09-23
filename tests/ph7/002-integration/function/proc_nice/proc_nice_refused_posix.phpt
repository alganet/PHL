--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
/* Root CAN raise the priority, so the refusal this test pins would not happen,
   and cmd.exe has no nice value at all -- php maps the increment onto the five
   Windows priority CLASSES there and never reports a failure. */
if (PHP_OS_FAMILY === "Windows" || getmyuid() === 0) {
    echo "skip needs a non-root POSIX host";
}
?>
--TEST--
proc_nice() warns and answers false when raising the priority is refused
--FILE--
<?php
/* Lowering the nice value raises the priority, which needs privilege. nice(3)
 * answers the NEW nice value and -1 is a legitimate one, so errno is the only
 * failure evidence -- which is why php clears it before the call and reads it
 * after, and why this is the one proc_nice() case a script can actually hit. */
set_error_handler(function ($n, $s) { echo "W: $s\n"; return true; });
var_dump(proc_nice(-5));
restore_error_handler();
?>
--EXPECT--
W: proc_nice(): Only a super user may attempt to increase the priority of a process
bool(false)
