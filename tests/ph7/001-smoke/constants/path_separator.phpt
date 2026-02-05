--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATH_SEPARATOR value
--FILE--
<?php
if (PHP_OS == 'WINNT') {
    if (PATH_SEPARATOR == ';') {
        echo "ok";
    } else {
        echo "not ok: " . PATH_SEPARATOR . " " . PHP_OS;
    }
} else {
    if (PATH_SEPARATOR == ':') {
        echo "ok";
    } else {
        echo "not ok: " . PATH_SEPARATOR . " " . PHP_OS;
    }
}
?>
--EXPECT--
ok
--CLEAN--
<?php

