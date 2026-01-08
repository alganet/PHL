--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strcasecmp with insufficient arguments (1 arg)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = strcasecmp("test");
if ($result === 1) {
    echo "PASS";
} else {
    echo "FAIL: expected 1, got " . var_export($result, true);
}
?>
--EXPECT--
PASS