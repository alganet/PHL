--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An UNCAUGHT throw raised inside a builtin ends the script; nothing runs after it
--DESCRIPTION--
The catchable side of this is pinned cross-engine by
001-smoke/function/mb/mb_encoding_throw_stops_call.phpt. Here the throw has no
handler at all: the fatal is reported and the script must stop with exit 255 —
the C routine reporting "ok" after raising used to let the executor fetch the
next instruction and run the rest of the program past the reported fatal.
--FILE--
<?php
echo "before\n";
mb_strlen("hello", "BOGUS");
echo "AFTER\n";
?>
--EXPECTF--
before
PHP Fatal error:  Uncaught ValueError: mb_strlen(): Argument #2 ($encoding) must be a valid encoding, "BOGUS" given in %s:3
Stack trace:
%A  thrown in %s on line 3
