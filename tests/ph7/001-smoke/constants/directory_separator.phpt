--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DIRECTORY_SEPARATOR value
--FILE--
<?php
if (PHP_OS == 'WINNT') {
    if (DIRECTORY_SEPARATOR == '\\') {
        echo "ok";
    } else {
        echo "not ok: " . DIRECTORY_SEPARATOR . " " . PHP_OS;
    }
} else {
    if (DIRECTORY_SEPARATOR == '/') {
        echo "ok";
    } else {
        echo "not ok: " . DIRECTORY_SEPARATOR . " " . PHP_OS;
    }
}
?>
--EXPECT--
ok
--CLEAN--
<?php

