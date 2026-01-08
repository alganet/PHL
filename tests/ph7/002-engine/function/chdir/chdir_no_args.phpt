--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: chdir() with no arguments returns FALSE
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Calling chdir with no arguments should return false
if (chdir() === false) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true