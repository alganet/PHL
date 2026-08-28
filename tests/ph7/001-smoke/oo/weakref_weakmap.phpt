--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: WeakReference and WeakMap (band D)
--FILE--
<?php
$o = new stdClass; $o->x = 1;
$w = WeakReference::create($o);
var_export($w->get() === $o); echo "\n";
unset($o);
var_export($w->get()); echo "\n";
try { WeakReference::create(5); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$m = new WeakMap();
$k = new stdClass;
$m[$k] = "val";
var_export(count($m)); echo "|", $m[$k], "|", isset($m[$k]) ? "y" : "n", "\n";
foreach ($m as $obj => $v) { var_export($obj === $k); echo "|$v\n"; }
unset($obj);
unset($k);
var_export(count($m)); echo "\n";
$k2 = new stdClass;
try { $m[$k2]; } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { $m["str"] = 1; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
var_export($m instanceof ArrayAccess && $m instanceof Countable && $m instanceof IteratorAggregate); echo "\n";
$m2 = new WeakMap(); $tmp = new stdClass; $m2[$tmp] = 9; unset($m2[$tmp]); var_export(count($m2)); echo "\n";
// value replacement + many refs
$a = new stdClass; $wA = WeakReference::create($a); $wB = WeakReference::create($a);
unset($a);
var_export($wA->get()); var_export($wB->get()); echo "\n";
// map key death releases only that entry
$p = new stdClass; $q = new stdClass;
$m3 = new WeakMap(); $m3[$p] = 1; $m3[$q] = 2;
unset($p);
var_export(count($m3)); echo "\n";
foreach ($m3 as $ko => $vo) { echo get_class($ko), "=", $vo, "\n"; }
?>
--EXPECTF--
true
NULL
WeakReference::create(): Argument #1 ($object) must be of type object, int given
1|val|y
true|val
0
Object stdClass#%d not contained in WeakMap
WeakMap key must be an object
true
0
NULLNULL
1
stdClass=2
--CLEAN--
<?php
unset($w, $m, $k2, $m2, $tmp, $wA, $wB, $m3, $q, $ko, $vo, $v);
