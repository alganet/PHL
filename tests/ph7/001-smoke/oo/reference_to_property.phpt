--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reference assignment to a property (instance, static, method-bound, clone)
--FILE--
<?php
// Instance property bound to a reference: writes flow both ways.
class RtpC { public $p; }
$x = 10;
$o = new RtpC;
$o->p = &$x;
$x = 20;
echo "read-through: {$o->p}\n";
$o->p = 30;
echo "write-back: {$x}\n";

// Static property bound to a reference (class-level).
class S { public static $s; }
$y = 1;
S::$s = &$y;
$y = 7;
echo "static read: " . S::$s . "\n";
S::$s = 8;
echo "static write-back: {$y}\n";

// Typed (mixed) property bound via a method, then source updated.
class RtpM { public mixed $r; function bind(&$v) { $this->r = &$v; } }
$m = new RtpM;
$w = 'a';
$m->bind($w);
$w = 'b';
echo "method-bound: {$m->r}\n";

// Clone preserves the reference binding (both alias the same variable).
class RtpD { public $q; }
$z = 100;
$d = new RtpD;
$d->q = &$z;
$e = clone $d;
$z = 200;
echo "clone shares ref: {$d->q} {$e->q}\n";
?>
--EXPECT--
read-through: 20
write-back: 30
static read: 7
static write-back: 8
method-bound: b
clone shares ref: 200 200
--CLEAN--
<?php
unset($x, $o, $y, $m, $w, $z, $d, $e);
