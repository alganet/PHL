--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array hash collision extended test
--FILE--
<?php
// Test array operations that trigger hash collision handling
// Use keys designed to potentially collide in hash table
$array = array();
$keys = array("a", "b", "c", "d", "e", "f", "g", "h", "i", "j");
foreach ($keys as $key) {
    $array[$key] = $key;
    $array[$key . "_dup"] = $key . "_dup";
}

// Perform operations that may trigger collision resolution
unset($array["b"]);
unset($array["d_dup"]);

var_dump(count($array));
var_dump(isset($array["a"]));
?>
--EXPECT--
int(18)
bool(true)
--CLEAN--
<?php
unset($array, $keys);
