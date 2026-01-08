--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
base_convert with no arguments
--FILE--
<?php
$result = base_convert();
if ($result == "") {
    echo "PASS\n";
} else {
    echo "FAIL: expected empty string, got " . var_export($result, true) . "\n";
}
?>
--EXPECT--
PASS
