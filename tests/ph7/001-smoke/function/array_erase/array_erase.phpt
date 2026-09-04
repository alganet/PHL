--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_erase should clear the content of the array
--SKIPIF--
<?php
// PHL extension: `array_erase()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: array_erase() is not a php symbol'; }
?>
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
array_erase($a);
echo count($a) . PHP_EOL; // 0
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a);
