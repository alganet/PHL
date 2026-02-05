--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var keyword outside class definition
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test the var statement outside of a class (PH7 extension)
var $outside_var = "Hello World";
var $another_var = 42;

echo $outside_var . "\n";
echo $another_var . "\n";
?>
--EXPECT--
Hello World
42
--CLEAN--
<?php

