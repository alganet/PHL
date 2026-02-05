--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DIR_SEP value
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
if (PHP_OS == 'WINNT') {
    if (DIR_SEP == '\\') {
        echo "ok";
    } else {
        echo "not ok: " . DIR_SEP . " " . PHP_OS;
    }
} else {
    if (DIR_SEP == '/') {
        echo "ok";
    } else {
        echo "not ok: " . DIR_SEP . " " . PHP_OS;
    }
}
?>
--EXPECT--
ok
--CLEAN--
<?php

