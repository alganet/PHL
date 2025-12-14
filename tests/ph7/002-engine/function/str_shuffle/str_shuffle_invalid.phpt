--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: str_shuffle missing argument returns empty string
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
if (str_shuffle() == "") {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
