--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_search with non-array third argument
$result1 = array_search("value", array("a", "b"), "not a bool");
echo "Non-bool strict: " . ($result1 === false ? "PASS" : "FAIL") . "\n";

// Test array_search with non-array haystack
$result2 = array_search("value", "not an array");
echo "Non-array haystack: " . ($result2 === false ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Non-bool strict: PASS
Non-array haystack: PASS
--CLEAN--
<?php
unset($result1, $result2);
