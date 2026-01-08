--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: octdec with integer argument
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = octdec(10);
if ($result === 10) {
    echo "PASS";
} else {
    echo "FAIL: expected 10, got " . var_export($result, true);
}
?>
--EXPECT--
PASS