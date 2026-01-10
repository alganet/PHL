--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on deeply nested arrays
--FILE--
<?php
// Test recursive counting to cover HashmapCount recursive path
$nested = array(
    "level1" => array(
        "level2" => array(
            "level3" => array(
                "level4" => array(1, 2, 3)
            )
        )
    ),
    "another" => array(
        array(4, 5),
        array(6, 7, 8, 9)
    )
);
$recursive_count = count($nested, 1); // COUNT_RECURSIVE
echo "Deep recursive count: " . $recursive_count . "\n";

// Test with mixed types
$mixed = array(
    "array" => array(1, 2, array(3, 4)),
    "string" => "test",
    "number" => 42,
    "nested" => array(
        "deep" => array(
            array(5),
            array(6, 7)
        )
    )
);
$mixed_count = count($mixed, 1);
echo "Mixed recursive count: " . $mixed_count . "\n";
?>
--EXPECT--
Deep recursive count: 16
Mixed recursive count: 15