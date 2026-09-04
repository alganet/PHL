--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_same should return TRUE if two variables reference the same array
--SKIPIF--
<?php
// PHL extension: `array_same()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: array_same() is not a php symbol'; }
?>
--FILE--
<?php
$a = array(1,2);
$b = $a; // In PH7 this keeps the same instance
$c = array(1,2);
echo array_same($a, $b) ? "ok\n" : "fail\n";
echo array_same($a, $c) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
fail
--CLEAN--
<?php
unset($a, $b, $c);
