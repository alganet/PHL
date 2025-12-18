--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: PHP_OS constant uname system call coverage
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
// Test that PHP_OS uses uname() system call rather than fallback
$os = PHP_OS;
if ($os === "Host OS") {
    echo "FAIL: Using fallback value\n";
} elseif (strlen($os) > 0) {
    echo "PASS: uname() returned: $os\n";
} else {
    echo "FAIL: Empty OS value\n";
}
?>
--EXPECTF--
PASS: uname() returned: %s