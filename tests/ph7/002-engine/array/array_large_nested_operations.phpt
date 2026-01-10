--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array large nested operations memory stress test
--FILE--
<?php
// Test complex nested array operations that may trigger memory allocation edge cases
$nested = array();
for ($i = 0; $i < 100; $i++) {
    $nested[] = array_fill(0, 100, array('deep' => array('nested' => str_repeat('x', 100))));
}
$result = count($nested);
echo "Created $result nested arrays\n";
?>
--EXPECT--
Created 100 nested arrays