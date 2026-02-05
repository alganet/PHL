--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
interface_exists builtin function basic checks
--SKIPIF--
<?php
if (!function_exists('interface_exists')) { echo "skip: function not available\n"; }
?>
--FILE--
<?php
interface Foo {}
// Should return true for existing interface
echo interface_exists('Foo') ? "ok\n" : "fail\n";
// Should return false for non-existing interface
echo interface_exists('Bar') ? "fail\n" : "ok\n";
// Interface names are case-insensitive
echo interface_exists('foo') ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
ok
ok
--CLEAN--
<?php

