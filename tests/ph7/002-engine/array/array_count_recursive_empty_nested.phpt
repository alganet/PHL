--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE with empty nested array
--FILE--
<?php
$nested = array(
    "a" => array(),
    "b" => array(1, 2)
);
$recursive_count = count($nested, 1); // COUNT_RECURSIVE
echo "Recursive: " . $recursive_count . "\n";
?>
--EXPECT--
Recursive: 4