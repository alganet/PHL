--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String comparison edge cases with empty strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test == operator with empty strings to cover SyStrncmp edge cases
echo ("" == "") ? "true" : "false";
echo "\n";
echo ("" == "a") ? "true" : "false";
echo "\n";
echo ("a" == "") ? "true" : "false";
echo "\n";
echo ("ab" == "ac") ? "true" : "false";
echo "\n";
echo ("a" == "a") ? "true" : "false";
echo "\n";
// Test strncmp with empty strings
echo strncmp("", "", 0);
echo "\n";
echo strncmp("", "a", 0);
echo "\n";
echo strncmp("a", "", 0);
echo "\n";
echo strncmp("ab", "ac", 2);
echo "\n";
?>
--EXPECT--
true
false
false
false
true
0
-1
1
-1
