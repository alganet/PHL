--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 only: PH7_VERSION constant defined
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PH7_VERSION not defined\n";
}
?>
--FILE--
<?php
echo defined('PH7_VERSION') ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
