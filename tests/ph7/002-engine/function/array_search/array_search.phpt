--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search basic functionality
--FILE--
<?php
$array = array('key1' => 'value1', 'key2' => 'value2', 'key3' => 'value3');

$result1 = array_search('value2', $array);
echo "Search for 'value2': " . ($result1 === 'key2' ? "PASS" : "FAIL") . "\n";

$result2 = array_search('notfound', $array);
echo "Search for 'notfound': " . ($result2 === false ? "PASS" : "FAIL") . "\n";

$result3 = array_search('value1', $array, true);
echo "Strict search for 'value1': " . ($result3 === 'key1' ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Search for 'value2': PASS
Search for 'notfound': PASS
Strict search for 'value1': PASS