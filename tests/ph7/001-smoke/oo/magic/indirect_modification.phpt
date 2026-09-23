--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An indirect modification of an overloaded property has no effect, and says so
--DESCRIPTION--
The property half of the overloaded-element rule. A write-context fetch of a name
only __get answers for gets a VALUE back, so php notices
`Indirect modification of overloaded property C::$p has no effect` and throws the
write away. PHL had the notice at one site only — a by-reference ARGUMENT — and
the value it handed out was not a copy: it still shared the object's own nested
array by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference foreach
modified the object php leaves untouched. A DECLARED but inaccessible name is
overloaded from outside the class in exactly the same way, where PHL refused the
whole statement with `Cannot access private property`. The by-ref argument path
had the notice but passed NULL rather than __get's value, which turned
`sort($o->a)` into a TypeError.
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "  [$no] $msg\n"; return true; });

class IndPropBox {
    private $d = ['a' => ['b' => 1], 'n' => 5];
    public function __get($k) { return $this->d[$k] ?? null; }
    public function __set($k, $v) { $this->d[$k] = $v; }
    public function __isset($k) { return isset($this->d[$k]); }
    public function peek($k) { return $this->d[$k] ?? 'MISSING'; }
}
function indPropFresh() { return new IndPropBox(); }
function indPropShow($label, $v) { echo $label, " => ", var_export($v, true), "\n"; }
function indPropByRef(&$x) { $x = 'WRITTEN'; }

$o = indPropFresh(); $o->a['b'] = 9;   indPropShow("nested ", $o->peek('a'));
$o = indPropFresh(); $o->a[] = 5;      indPropShow("append ", $o->peek('a'));
$o = indPropFresh(); $o->a['b']++;     indPropShow("incr   ", $o->peek('a'));
$o = indPropFresh(); $r = &$o->a; $r['b'] = 9;
                                       indPropShow("ref    ", $o->peek('a'));
$o = indPropFresh(); foreach ($o->a as &$v) { $v = 'X'; } unset($v);
                                       indPropShow("foreach", $o->peek('a'));

// A by-reference ARGUMENT is passed BY VALUE — __get's value, which the callee
// then operates on for nothing.
$o = indPropFresh(); indPropByRef($o->a);  indPropShow("byref arg", $o->peek('a'));
$o = indPropFresh(); sort($o->a);          indPropShow("sort     ", $o->peek('a'));

// A plain READ is not a write fetch: no notice, and a copy nobody shares.
$o = indPropFresh();
$copy = $o->a;
$copy['b'] = 42;
indPropShow("read   ", $o->peek('a'));

// A DECLARED but inaccessible name is overloaded from out here just the same.
class IndPropPrivate {
    private $a = ['b' => 1];
    public function __get($k) { return $this->a; }
    public function __set($k, $v) {}
    public function peek() { return $this->a; }
}
$p = new IndPropPrivate(); $p->a['b'] = 9;  indPropShow("private nested ", $p->peek());
$p = new IndPropPrivate(); foreach ($p->a as &$w) { $w = 'X'; } unset($w);
                                            indPropShow("private foreach", $p->peek());

// A hooked property is php's Error, not a notice: a `get` hook is not an
// accessor php will silently discard a write through.
class IndPropHooked {
    private $s = ['b' => 1];
    public array $h { get { return $this->s; } set { $this->s = $value; } }
}
$h = new IndPropHooked();
try {
    $h->h['b'] = 9;
} catch (Error $e) {
    echo "hooked: ", $e->getMessage(), "\n";
}
indPropShow("hooked ", $h->h);
?>
--EXPECT--
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
nested  => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
append  => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
incr    => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
ref     => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
foreach => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
byref arg => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropBox::$a has no effect
sort      => array (
  'b' => 1,
)
read    => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropPrivate::$a has no effect
private nested  => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded property IndPropPrivate::$a has no effect
private foreach => array (
  'b' => 1,
)
hooked: Indirect modification of IndPropHooked::$h is not allowed
hooked  => array (
  'b' => 1,
)
