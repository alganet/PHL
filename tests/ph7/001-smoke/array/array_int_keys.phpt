--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with integer keys
--FILE--
<?php
$a = array();
for ($i = 0; $i < 100; $i++) {
    $a[$i] = "value_$i";
}
echo "Count: " . count($a) . "\n";
echo "First: " . $a[0] . "\n";
echo "Last: " . $a[99] . "\n";
?>
--EXPECT--
Count: 100
First: value_0
Last: value_99
--CLEAN--
<?php
unset($a);
