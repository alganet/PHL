--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parse error in an included file is a catchable ParseError
--DESCRIPTION--
php 8 makes an include/require whose compile fails throw a ParseError naming the
OFFENDING file and line. This engine printed the diagnostic, answered false and
CARRIED ON -- so the half-compiled unit stayed behind for later code to trip over,
which is how a parse error deep in a class body surfaced as a wrong runtime answer
somewhere else entirely (ECOSYSTEM.md F30/F14).
--FILE--
<?php
echo "before\n";
try {
    require __DIR__ . '/include_parse_error.inc';
    echo "unreached\n";
} catch (ParseError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
    echo basename($e->getFile()), ':', $e->getLine(), "\n";
}
/* An include is an expression, and the throw unwinds the statement it is in. */
try {
    $r = 1 + include __DIR__ . '/include_parse_error.inc';
    echo "unreached\n";
} catch (ParseError $e) {
    echo "second: ", $e->getMessage(), "\n";
}
echo "after\n";
?>
--EXPECT--
before
ParseError: syntax error, unexpected token ";"
include_parse_error.inc:3
second: syntax error, unexpected token ";"
after
