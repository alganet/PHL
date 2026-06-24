--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: nested delegation
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function a() { yield 1; yield 2; }
function b() { yield 0; yield from a(); yield 3; }
function c() { yield from b(); yield 4; }
echo implode(",", iterator_to_array(c(), false)), "\n";
?>
--EXPECT--
0,1,2,3,4
--CLEAN--
<?php
