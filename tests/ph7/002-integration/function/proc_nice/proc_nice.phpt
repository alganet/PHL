--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
proc_nice() accepts a priority change the platform allows
--FILE--
<?php
/* php's proc_nice() answers TRUE when the platform accepted the change, and its
 * POSIX failure is the one a script can actually hit: lowering the nice value
 * (raising the priority) needs privilege. nice(3)'s own answer is the NEW nice
 * value, and -1 is a legitimate one, so errno is the only failure evidence --
 * which is why php clears it before the call and reads it after. */
var_dump(proc_nice(0));
var_dump(proc_nice(1));
?>
--EXPECT--
bool(true)
bool(true)
