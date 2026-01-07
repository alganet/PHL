--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation edge cases
--FILE--
<?php
// Test complex string interpolation to exercise uncovered code paths
$a = array('key' => 'value');
$name = 'world';
$complex = "Hello {$name}, array access: {$a['key']}, nested: {${'name'}}";
$another = "Test: {$a["key"]}";
echo $complex . "\n";
echo $another . "\n";
?>
--EXPECT--
Hello world, array access: value, nested: world
Test: value