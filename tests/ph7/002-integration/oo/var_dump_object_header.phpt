--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump object header is object(Class)#id (propCount); static/const excluded from count
--DESCRIPTION--
PHL-only: the var_dump header now carries the object id (#N) and the property
count, but the body keeps PHL's existing format (single-quoted keys, 1-space
indent) — full var_dump byte-fidelity is a separate deferred task.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class C { public $x = 1; public $y = "two"; public static $s = 9; const K = 5; }
var_dump(new C);
?>
--EXPECT--
object(C)#1 (2) {
 ['x'] =>
  int(1)
 ['y'] =>
  string(3) "two"
 }
--CLEAN--
<?php
