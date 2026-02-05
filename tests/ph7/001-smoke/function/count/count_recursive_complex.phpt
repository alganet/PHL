--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on deeply nested arrays
--FILE--
<?php
// Test recursive count on deeply nested arrays to cover hashmap.c recursive path
$nested = array(
    "level1" => array(
        "level2" => array(
            "level3" => array(
                "level4" => array(1, 2, 3, 4, 5)
            ),
            "another" => array(6, 7)
        ),
        "simple" => array(8, 9, 10)
    ),
    "top" => array(11, 12)
);

$normal_count = count($nested);
$recursive_count = count($nested, COUNT_RECURSIVE);

echo "Normal count: " . $normal_count . "\n";
echo "Recursive count: " . $recursive_count . "\n";

// Also test with mixed types
$mixed = array(
    "strings" => array("a", "b", "c"),
    "numbers" => array(1, 2, array(3, 4)),
    "empty" => array()
);

$mixed_normal = count($mixed);
$mixed_recursive = count($mixed, COUNT_RECURSIVE);

echo "Mixed normal: " . $mixed_normal . "\n";
echo "Mixed recursive: " . $mixed_recursive . "\n";
?>
--EXPECT--
Normal count: 2
Recursive count: 19
Mixed normal: 3
Mixed recursive: 11
--CLEAN--
<?php
unset($nested, $normal_count, $recursive_count, $mixed, $mixed_normal, $mixed_recursive);
