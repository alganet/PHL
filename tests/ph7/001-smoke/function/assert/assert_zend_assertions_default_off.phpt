--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zend.assertions defaults to -1 on the CLI so assert() is a no-op
--SKIPIF--
skip: macos
--FILE--
<?php
echo "zend=" . ini_get("zend.assertions") . "\n";
echo assert(false) ? "true\n" : "false\n";   // compiled out: returns true, no throw
echo "reached\n";
?>
--EXPECT--
zend=-1
true
reached
--CLEAN--
<?php
