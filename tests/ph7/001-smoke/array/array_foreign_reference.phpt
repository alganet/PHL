--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Array foreign reference behavior - unsetting referenced variable removes array element
--FILE--
<?php
// Test PHL-specific foreign reference behavior
// When a variable is added to an array by reference and then unset,
// the array element should be automatically removed
$var = 10;
$a = array();
$a[] =& $var;
echo count($a) . PHP_EOL; // Should be 1
// Unset the foreign ph7_value now
unset($var);
echo count($a) . PHP_EOL; // Should be 0 due to PHL foreign reference handling
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($var, $a);
