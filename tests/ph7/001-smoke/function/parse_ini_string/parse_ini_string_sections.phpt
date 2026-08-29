--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: parse_ini_string with process_sections = true

--FILE--
<?php
$ini = "global_key = global_value\n[section1]\nkey1 = value1\nkey2 = value2\n[section2]\nkey3 = value3";
$result = parse_ini_string($ini, true);
echo "global_key: " . $result['global_key'] . "\n";
echo "section1 key1: " . $result['section1']['key1'] . "\n";
echo "section1 key2: " . $result['section1']['key2'] . "\n";
echo "section2 key3: " . $result['section2']['key3'] . "\n";
echo "Count: " . count($result) . "\n";
?>
--EXPECT--
global_key: global_value
section1 key1: value1
section1 key2: value2
section2 key3: value3
Count: 3
--CLEAN--
<?php
unset($ini, $result);
