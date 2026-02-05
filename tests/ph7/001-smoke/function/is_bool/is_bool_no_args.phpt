--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: is_bool() with no arguments returns false (covers uncovered branch)
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Calling is_bool with no arguments should return false (0)
// This covers the uncovered branch: if( nArg > 0 ) in builtin.c
if (is_bool() === false) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
--CLEAN--
<?php

