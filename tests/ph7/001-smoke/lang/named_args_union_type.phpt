--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: union type hints with out-of-order args
--FILE--
<?php
function nautf(int|string $x, float|int $y) {
    $tx = is_int($x) ? "int" : (is_string($x) ? "string" : "?");
    $ty = is_int($y) ? "int" : (is_float($y) ? "float" : "?");
    echo "$tx:$x $ty:$y\n";
}
nautf(y: 3, x: "hello");
nautf(y: 2.5, x: 42);
nautf(x: "test", y: 100);
?>
--EXPECT--
string:hello int:3
int:42 float:2.5
string:test int:100
--CLEAN--
<?php
