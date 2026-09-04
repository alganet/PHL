--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: implode_recursive basic functionality
--SKIPIF--
<?php
// PHL extension: `implode_recursive()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: implode_recursive() is not a php symbol'; }
?>
--FILE--
<?php
// Test basic implode_recursive functionality
$result1 = implode_recursive("-", "test");
echo $result1 === "test" ? "SIMPLE_OK\n" : "SIMPLE_FAIL: '$result1'\n";

// Test with glue and single string
$result2 = implode_recursive("/", "hello");
echo $result2 === "hello" ? "GLUE_OK\n" : "GLUE_FAIL: '$result2'\n";

// Test that function exists and returns string
$result3 = implode_recursive(",", "abc");
echo is_string($result3) ? "TYPE_OK\n" : "TYPE_FAIL\n";

// Test join_recursive (alias)
$result4 = join_recursive("-", "test");
echo $result4 === "test" ? "ALIAS_OK\n" : "ALIAS_FAIL: '$result4'\n";
?>
--EXPECT--
SIMPLE_OK
GLUE_OK
TYPE_OK
ALIAS_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4);
