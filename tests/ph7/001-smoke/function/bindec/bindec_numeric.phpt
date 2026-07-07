--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindec with numeric argument
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test bindec with numeric argument (covers uncovered line 1003)
$result1 = bindec(15);
echo "bindec(15): " . $result1 . "\n";

// Test bindec with string (normal case)
$result2 = bindec("1010");
echo "bindec('1010'): " . $result2 . "\n";
?>
--EXPECT--
bindec(15): 15
bindec('1010'): 10
--CLEAN--
<?php
unset($result1, $result2);
