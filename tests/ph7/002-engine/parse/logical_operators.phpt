--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Test PH7 logical operators with lower precedence
--FILE--
<?php
// Test 'and' operator (lower precedence than &&)
$result1 = true and false;
var_dump($result1); // bool(false)

$result2 = true and true;
var_dump($result2); // bool(true)

// Test 'or' operator (lower precedence than ||)
$result3 = false or true;
var_dump($result3); // bool(true)

$result4 = false or false;
var_dump($result4); // bool(false)

// Test 'xor' operator
$result5 = true xor false;
var_dump($result5); // bool(true)

$result6 = true xor true;
var_dump($result6); // bool(false)

$result7 = false xor false;
var_dump($result7); // bool(false)

// Test mixed precedence
$result8 = false || true and false;
var_dump($result8); // bool(false) - and has lower precedence

$result9 = true and false || true;
var_dump($result9); // bool(true) - or has lower precedence than and
?>
--EXPECT--
bool(TRUE)
bool(TRUE)
bool(FALSE)
bool(FALSE)
bool(TRUE)
bool(TRUE)
bool(FALSE)
bool(TRUE)
bool(TRUE)