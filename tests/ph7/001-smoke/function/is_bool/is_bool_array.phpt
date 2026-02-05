--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: is_bool() with array argument returns false
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Calling is_bool with an array should return false
$array = array(1, 2, 3);
if (is_bool($array) === false) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
--CLEAN--
<?php
unset($array);
