--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object type hint selects correct overload
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class ObjOverA {}

function objOverDispatch(object $o) { return "object"; }
function objOverDispatch(string $s) { return "string"; }
function objOverDispatch(int $i)    { return "int"; }

echo objOverDispatch(new ObjOverA()) . "\n";
echo objOverDispatch("hello") . "\n";
echo objOverDispatch(42) . "\n";
?>
--EXPECT--
object
string
int
--CLEAN--
<?php

