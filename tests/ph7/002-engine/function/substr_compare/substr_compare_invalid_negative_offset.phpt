--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare with invalid negative offset
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = substr_compare("a", "b", -2);
if ($result === false) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS