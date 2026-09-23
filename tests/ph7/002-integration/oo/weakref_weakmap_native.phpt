--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP: WeakReference identity/instantiation and WeakMap's live InternalIterator
--FILE--
<?php
$a = new stdClass;

/* One WeakReference per target, handed back every time */
$w = WeakReference::create($a);
var_dump(WeakReference::create($a) === $w);

/* The class exists only through create() and cannot be copied */
try { new WeakReference(); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { (new ReflectionClass('WeakReference'))->newInstance(); }
catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { clone $w; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump((new ReflectionClass('WeakReference'))->isFinal(),
         (new ReflectionClass('WeakMap'))->isFinal());

/* Declared parameters reach Reflection, and arity is enforced both ways */
$r = new ReflectionMethod('WeakReference', 'create');
var_dump($r->isStatic(), count($r->getParameters()), $r->getParameters()[0]->getName());
try { $w->get(1); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { WeakReference::create(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { WeakReference::create(1); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { spl_object_id("x"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

/* getIterator() answers php's InternalIterator, already positioned */
$m = new WeakMap;
$b = new stdClass; $c = new stdClass;
$m[$a] = 'A'; $m[$b] = 'B'; $m[$c] = 'C';
$it = $m->getIterator();
echo get_class($it), "\n";
var_dump($it->valid(), $it->current(), $it->key() === $a);
try { $m->count(1); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }

/* The walk is LIVE: an entry removed ahead of the cursor is never reached ... */
$s = '';
foreach ($m as $k => $v) { $s .= $v; unset($m[$c]); }
echo $s, "\n";

/* ... one appended while the loop stands on the last entry still is ... */
$m2 = new WeakMap;
$d = new stdClass; $e2 = new stdClass;
$m2[$d] = 'D';
$keep = [];
foreach ($m2 as $v) {
	$s = ($s ?? '') . $v;
	if (count($keep) < 2) { $n = new stdClass; $keep[] = $n; $m2[$n] = 'N'; }
}
echo $s, "\n";

/* ... and two walks of the same map keep independent cursors */
$m3 = new WeakMap;
$m3[$a] = 'A'; $m3[$b] = 'B';
$s = '';
foreach ($m3 as $v1) { foreach ($m3 as $v2) { $s .= $v1 . $v2 . ','; } }
echo $s, "\n";

/* A cloned map is a separate map over the same targets */
$m4 = new WeakMap;
$m4[$a] = 'A'; $m4[$b] = 'B';
$cl = clone $m4;
$cl[$c] = 'C';
echo count($m4), count($cl), $cl[$a], "\n";
unset($cl[$a]);
echo count($m4), count($cl), "\n";

/* Death of a key prunes the entry, and the shared WeakReference sees it too */
$m5 = new WeakMap;
$f = new stdClass;
$m5[$f] = 'F';
$wf = WeakReference::create($f);
unset($f);
var_dump(count($m5), $wf->get());
?>
--EXPECTF--
bool(true)
Error: Direct instantiation of WeakReference is not allowed, use WeakReference::create instead
Error: Direct instantiation of WeakReference is not allowed, use WeakReference::create instead
Error: Trying to clone an uncloneable object of class WeakReference
bool(true)
bool(true)
bool(true)
int(1)
string(6) "object"
WeakReference::get() expects exactly 0 arguments, 1 given
WeakReference::create() expects exactly 1 argument, 0 given
WeakReference::create(): Argument #1 ($object) must be of type object, int given
spl_object_id(): Argument #1 ($object) must be of type object, string given
InternalIterator
bool(true)
string(1) "A"
bool(true)
WeakMap::count() expects exactly 0 arguments, 1 given
AB
ABDNN
AA,AB,BA,BB,
23A
22
int(0)
NULL
--CLEAN--
<?php
unset($w, $r, $it, $m, $m2, $m3, $m4, $m5, $a, $b, $c, $d, $e2, $cl, $wf, $keep, $n, $s, $k, $v, $v1, $v2);
