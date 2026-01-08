--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strncmp with insufficient arguments falls back to strcmp
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with 2 arguments - should behave like strcmp
$result1 = strncmp("abc", "abc");
$result2 = strncmp("abc", "abd");
if ($result1 === 0 && $result2 < 0) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS