--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An uncaught ParseError is php's `Parse error:` line, and the program stops
--DESCRIPTION--
php renders an uncaught ParseError as the E_PARSE it stands for -- one plain line
naming the file and the line, with no "Uncaught", no stack trace and no "thrown in"
trailer -- and exits 255. (CompileError is the same shape under "Fatal error".)
A user SUBCLASS of either is an ordinary uncaught exception, which is why php
matches the class by identity and so does this.
--FILE--
<?php
echo "before\n";
require __DIR__ . '/include_parse_error.inc';
echo "unreached\n";
?>
--EXPECTF--
before
%AParse error:  syntax error, unexpected token ";" in %sinclude_parse_error.inc on line 3%A
