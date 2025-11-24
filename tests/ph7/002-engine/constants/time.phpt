--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 only: __TIME__ expands to HH:MM:SS
--SKIPIF--
<?php
if (!defined('__TIME__')) {
    echo "skip __TIME__ not defined\n";
}
?>
--FILE--
<?php
// __TIME__ expands to HH:MM:SS — print and validate with regex
$val = __TIME__;
// Validate format without relying on regex: HH:MM:SS
if (strlen($val) === 8 && $val[2] === ':' && $val[5] === ':') {
    echo "ok\n";
} else {
    echo "fail\n";
}
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($val);
?>
