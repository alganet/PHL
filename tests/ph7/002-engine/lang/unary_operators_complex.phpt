--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex unary operators
--FILE--
<?php
$a = 5;
$b = -$a++;
echo "b: " . $b . "\n"; // -5, then a becomes 6
$c = ++$a;
echo "c: " . $c . "\n"; // 7
$d = ~$c;
echo "d: " . $d . "\n"; // bitwise not
$e = !$d;
echo "e: " . ($e ? "true" : "false") . "\n"; // logical not, true since ~7 != 0
?>
--EXPECT--
b: -5
c: 7
d: -8
e: false
--CLEAN--
<?php
unset($a, $b, $c, $d, $e);
?>