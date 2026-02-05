--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex nested ternary expressions to exercise parsing logic
--FILE--
<?php
// Test nested ternary expressions
$a = 5;
$b = 10;
$c = 15;

$result = $a > 3 ? ($b < 20 ? ($c == 15 ? "nested_true" : "nested_false") : "middle_false") : "outer_false";

echo $result . "\n";

// Another complex expression
$d = true;
$e = false;
$f = null;

$result2 = $d ? ($e ? "de_true" : ($f ? $f : "f_null")) : "d_false";

echo $result2 . "\n";
?>
--EXPECT--
nested_true
f_null
--CLEAN--
<?php
unset($a, $b, $c, $result, $d, $e, $f, $result2);
