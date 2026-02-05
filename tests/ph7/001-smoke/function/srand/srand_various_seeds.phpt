--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
srand with various seed values
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Test srand with negative seed
srand(-12345);
$r1 = rand();
echo "neg_seed=" . (is_int($r1) ? 'int' : 'not_int') . PHP_EOL;

// Test srand with large seed
srand(999999999);
$r2 = rand();
echo "large_seed=" . (is_int($r2) ? 'int' : 'not_int') . PHP_EOL;

// Test srand with float seed (should truncate)
srand(42.7);
$r3 = rand();
echo "float_seed=" . (is_int($r3) ? 'int' : 'not_int') . PHP_EOL;

// Test srand with string number
srand("54321");
$r4 = rand();
echo "string_seed=" . (is_int($r4) ? 'int' : 'not_int') . PHP_EOL;
?>
--EXPECT--
neg_seed=int
large_seed=int
float_seed=int
string_seed=int
--CLEAN--
<?php
unset($r1, $r2, $r3, $r4);
