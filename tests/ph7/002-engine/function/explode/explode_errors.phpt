--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: explode function error cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test explode with empty delimiter
var_dump(explode("", "hello"));

// Test explode with empty string
var_dump(explode(",", ""));

// Test explode with normal case
$result = explode(",", "a,b,c");
var_dump($result);
?>
--EXPECT--
bool(FALSE)
bool(FALSE)
array(3) {
 [0] =>
  string(1 'a')
 [1] =>
  string(1 'b')
 [2] =>
  string(1 'c')
 }
--CLEAN--
<?php
// Cleanup if needed
unset($result);
?>
