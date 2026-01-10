--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation with curly braces
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test string interpolation with curly braces to cover line 918 in compile.c
$name = "world";
$greeting = "Hello {$name}!";
echo $greeting . "\n";

// Test nested braces
$data = array("user" => array("name" => "Alice"));
$message = "Welcome {$data['user']['name']}!";
echo $message . "\n";
?>
--EXPECT--
Hello world!
Welcome Alice!