--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 only: __DATE__ expands to YYYY-MM-DD
--SKIPIF--
<?php
if (!defined('__DATE__')) {
    echo "skip __DATE__ not defined\n";
}
?>
--FILE--
<?php
$val = __DATE__;
// Validate format YYYY-MM-DD without regex
if (strlen($val) === 10 && $val[4] === '-' && $val[7] === '-') {
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
