--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: hashmap_simple_key with string key coverage

--FILE--
<?php
// Test key() function with string key
$arr = array("name" => "value", "age" => 25);
echo key($arr) . "\n";
next($arr);
echo key($arr) . "\n";
?>
--EXPECT--
name
age
--CLEAN--
<?php
unset($arr);
