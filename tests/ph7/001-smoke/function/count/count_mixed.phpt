--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with mixed array types
--FILE--
<?php
$mixed = array(
    'string' => 'value',
    0 => 123,
    'nested' => array(1, 2, 3),
    'bool' => true,
    'null' => null
);
$normal_count = count($mixed);
$recursive_count = count($mixed, COUNT_RECURSIVE);
echo "Normal: " . $normal_count . "\n";
echo "Recursive: " . $recursive_count . "\n";
?>
--EXPECT--
Normal: 5
Recursive: 8
--CLEAN--
<?php
unset($mixed, $normal_count, $recursive_count);
