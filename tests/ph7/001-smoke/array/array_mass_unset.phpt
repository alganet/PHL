--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with mass unset to exercise hashmap deletion
--FILE--
<?php
/* Create an array with many elements */
$a = array();
for ($i = 0; $i < 500; $i++) {
    $a["key_$i"] = "value_$i";
}
echo "Initial count: " . count($a) . "\n";

/* Unset many elements to exercise hashmap deletion */
for ($i = 0; $i < 500; $i += 2) {
    unset($a["key_$i"]);
}
echo "After mass unset: " . count($a) . "\n";

/* Add some back */
for ($i = 0; $i < 10; $i++) {
    $a["new_key_$i"] = "new_value_$i";
}
echo "After adding back: " . count($a) . "\n";

/* Check some remaining elements */
echo "Check remaining: " . (isset($a["key_1"]) ? "yes" : "no") . "\n";
echo "Check unset: " . (isset($a["key_0"]) ? "yes" : "no") . "\n";
echo "Check new: " . $a["new_key_5"] . "\n";

echo "Test completed\n";
?>
--EXPECT--
Initial count: 500
After mass unset: 250
After adding back: 260
Check remaining: yes
Check unset: no
Check new: new_value_5
Test completed
--CLEAN--
<?php
unset($a);
