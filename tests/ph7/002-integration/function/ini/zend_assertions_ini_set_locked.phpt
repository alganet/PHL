--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_set('zend.assertions') is rejected while the value is -1 (php.ini-only switch)
--SKIPIF--
skip: macos
--FILE--
<?php
$r = ini_set("zend.assertions", "1");
var_dump($r);
echo ini_get("zend.assertions") . "\n";
?>
--EXPECTF--
%Abool(false)
-1
--CLEAN--
<?php
