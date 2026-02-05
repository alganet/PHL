--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count function with recursive counting
--FILE--
<?php
// Test recursive count
$array = array(
    array(1, 2, 3),
    array(4, 5),
    6
);
echo count($array) . "\n"; // Should be 3
echo count($array, 1) . "\n"; // Should be 3 + 5 = 8 (recursive count)
?>
--EXPECT--
3
8
--CLEAN--
<?php
unset($array);
