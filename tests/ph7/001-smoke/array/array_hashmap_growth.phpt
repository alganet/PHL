--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations that trigger hashmap growth
--FILE--
<?php
/* Create an array with many elements to trigger hashmap bucket growth */
$a = array();
for ($i = 0; $i < 200; $i++) {
    $a["key_$i"] = "value_$i";
}
/* Perform operations that may exercise hashmap internals */
echo "Initial count: " . count($a) . "\n";
/* Add more elements */
for ($i = 200; $i < 400; $i++) {
    $a["key_$i"] = "value_$i";
}
echo "After adding: " . count($a) . "\n";
/* Access elements */
echo "Access test: " . $a["key_50"] . "\n";
/* Modify elements */
$a["key_100"] = "modified";
echo "Modify test: " . $a["key_100"] . "\n";
/* Unset elements */
unset($a["key_150"]);
echo "After unset: " . count($a) . "\n";
echo "Test completed\n";
?>
--EXPECT--
Initial count: 200
After adding: 400
Access test: value_50
Modify test: modified
After unset: 399
Test completed
--CLEAN--
<?php
unset($a);
