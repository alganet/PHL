--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
LOCK_UN constant expands to 3
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip"; }
?>
--FILE--
<?php
echo LOCK_UN . "\n";
?>
--EXPECT--
0