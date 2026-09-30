--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sys_getloadavg(): three floats under integer keys, and absent where php's is
--DESCRIPTION--
php builds sys_getloadavg() only where the C library has getloadavg(), which is not
Windows -- so `function_exists('sys_getloadavg')` is FALSE there, which is what a
program guarding its use looks for (monolog's LoadAverageProcessor does exactly that
before asking for it on every record). The VALUES are the machine's, so only their
shape is pinned: a three-element list under keys 0/1/2, every one of them a float.
--SKIPIF--
<?php
if (!function_exists('sys_getloadavg')) { echo 'skip no getloadavg() on this platform (Windows)'; }
?>
--FILE--
<?php
$l = sys_getloadavg();
var_dump(is_array($l), count($l), array_keys($l));
var_dump(is_float($l[0]), is_float($l[1]), is_float($l[2]));
$r = new ReflectionFunction('sys_getloadavg');
var_dump($r->getNumberOfParameters(), (string)$r->getReturnType(), $r->isInternal());
var_dump(in_array('sys_getloadavg', get_extension_funcs('standard'), true));
?>
--EXPECT--
bool(true)
int(3)
array(3) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
bool(true)
bool(true)
bool(true)
int(0)
string(11) "array|false"
bool(true)
bool(true)
