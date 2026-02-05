--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with mixed key types and lookups
--FILE--
<?php
// Test mixed key types and lookup operations
$a = array();
$a[0] = 'zero';
$a['1'] = 'one';
$a[2.0] = 'two';
$a[] = 'auto';

echo count($a) . ' ';
echo $a[0] . ' ';
echo $a['1'] . ' ';
echo $a[2] . ' ';
echo $a[3] . PHP_EOL;

// Test lookup operations
echo array_key_exists(0, $a) ? '1' : '0';
echo array_key_exists('1', $a) ? '1' : '0';
echo array_key_exists(2, $a) ? '1' : '0';
echo array_key_exists('nonexistent', $a) ? '1' : '0';
echo PHP_EOL;
?>
--EXPECT--
4 zero one two auto
1110
--CLEAN--
<?php
unset($a);
