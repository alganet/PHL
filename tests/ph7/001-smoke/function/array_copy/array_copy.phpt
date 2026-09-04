--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_copy should return a deep copy that is independent from the original
--SKIPIF--
<?php
// PHL extension: `array_copy()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: array_copy() is not a php symbol'; }
?>
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
$b = array_copy($a);
$b['x'] = 42;
echo $a['x'] . PHP_EOL; // 1
echo $b['x'] . PHP_EOL; // 42
?>
--EXPECT--
1
42
--CLEAN--
<?php
unset($a, $b);
