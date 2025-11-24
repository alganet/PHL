--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_EOL value (ordinal of first char)
--FILE--
<?php
$ord = ord(PHP_EOL);
if (PHP_OS == 'WINNT') {
    if ($ord == 13) {
        echo "ok";
    } else {
        echo "not ok: " . $ord . " " . PHP_OS;
    }
} else {
    if ($ord == 10) {
        echo "ok";
    } else {
        echo "not ok: " . $ord . " " . PHP_OS;
    }
}
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($ord);
?>
