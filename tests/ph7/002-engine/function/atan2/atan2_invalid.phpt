--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: atan2 missing arguments returns integer 0
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Calling atan2 with no arguments should return integer 0
if (atan2() === 0) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
