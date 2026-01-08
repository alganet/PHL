--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf with %c format specifier
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result1 = sprintf("%c", 65);
if ($result1 === "A") {
    echo "PASS_BASIC\n";
} else {
    echo "FAIL_BASIC: expected 'A', got " . var_export($result1, true) . "\n";
}

// Test with missing argument (should use 0 -> NUL)
$result2 = sprintf("%c");
if ($result2 === "\0") {
    echo "PASS_MISSING_ARG\n";
} else {
    echo "FAIL_MISSING_ARG: expected NUL, got " . var_export($result2, true) . "\n";
}
?>
--EXPECT--
PASS_BASIC
PASS_MISSING_ARG