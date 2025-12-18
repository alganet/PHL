--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: is_null() with no arguments returns false
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Calling is_null with no arguments should return false
if (is_null() === false) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
