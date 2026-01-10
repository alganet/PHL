--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array hash collision handling
--FILE--
<?php
// Test array operations that may trigger hash collisions
$a = array();
for ($i = 0; $i < 100; $i++) {
    $a["key" . $i] = $i;
    $a["collision" . ($i * 31)] = "collide" . $i; // Potential hash collision
}
// Test deletion and re-insertion
unset($a["key50"]);
$a["key50"] = 500;
// Test access
echo $a["key50"] . "\n";
echo count($a) . "\n";
?>
--EXPECT--
500
200