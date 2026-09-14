--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex nested expression parsing

--FILE--
<?php
// Test complex nested expressions that may trigger edge cases in parsing.
// (Kept integer end-to-end: PHL rejects a lossy float->int operand for %, so the
// nesting is exercised without leaning on that removed coercion.)
$a = 1;
$b = 2;
$c = 3;
$result = (($a + $b) * ($c - $a) + ($b + $c)) % 5;
echo $result . "\n";
// Test with function calls and operators
$d = array(1, 2, 3);
$e = count($d) + strlen("test") - (int)(3.14 * 2);
echo $e . "\n";
?>
--EXPECT--
1
1
--CLEAN--
<?php
unset($a, $b, $c, $result, $d, $e);
