--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An indirect modification of an overloaded element has no effect, and says so
--DESCRIPTION--
A write-context fetch on an ArrayAccess container asks it for something to MODIFY.
A userland offsetGet can only hand back a VALUE, so php notices
`Indirect modification of overloaded element of C has no effect` and throws the
write away. PHL had neither half: the notice was missing, and the value it handed
out still shared the container's own nested array by COW, so `$o['a']['b'] = 9`,
`$o['a'][] = 5` and a by-reference foreach all modified the object php leaves
untouched — silently. php's split is the read_dimension handler, not the
interface: ArrayObject, ArrayIterator and WeakMap hand back the real element and
DO support indirect modification, while SplFixedArray, the SplDoublyLinkedList
family and SplObjectStorage keep the standard handler and get the notice like any
userland class. An override of offsetGet takes a subclass off the fast handler in
php too.
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "  [$no] $msg\n"; return true; });

class IndModBox implements ArrayAccess {
    public $d;
    public function __construct($d) { $this->d = $d; }
    public function offsetExists($o): bool { return isset($this->d[$o]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($o) { return $this->d[$o] ?? null; }
    public function offsetSet($o, $v): void { if ($o === null) { $this->d[] = $v; } else { $this->d[$o] = $v; } }
    public function offsetUnset($o): void { unset($this->d[$o]); }
}
function indModFresh() { return new IndModBox(['a' => ['b' => 1], 'n' => 5]); }
function indModShow($label, $v) { echo $label, " => ", var_export($v, true), "\n"; }

$o = indModFresh(); $o['a']['b'] = 9;    indModShow("nested ", $o->d['a']);
$o = indModFresh(); $o['a'][] = 5;       indModShow("append ", $o->d['a']);
$o = indModFresh(); $o['a']['b']++;      indModShow("incr   ", $o->d['a']);
$o = indModFresh(); $o['n']++;           indModShow("scalar ", $o->d['n']);
$o = indModFresh(); unset($o['a']['b']); indModShow("unset  ", $o->d['a']);
$o = indModFresh(); $r = &$o['a']; $r['b'] = 9;
                                         indModShow("ref    ", $o->d['a']);
$o = indModFresh(); foreach ($o['a'] as &$v) { $v = 'X'; } unset($v);
                                         indModShow("foreach", $o->d['a']);

// A plain READ is not a write fetch: no notice, and a copy nobody shares.
$o = indModFresh();
$copy = $o['a'];
$copy['b'] = 42;
indModShow("read   ", $o->d['a']);

// The SPL containers php keeps on the standard handler answer the same way.
$f = new SplFixedArray(1); $f[0] = ['b' => 1];
$f[0]['b'] = 9;
indModShow("SplFixedArray   ", $f[0]);
$s = new SplStack(); $s->push(['b' => 1]);
$s[0]['b'] = 9;
indModShow("SplStack        ", $s[0]);
$so = new SplObjectStorage(); $k = new stdClass(); $so[$k] = ['b' => 1];
$so[$k]['b'] = 9;
indModShow("SplObjectStorage", $so[$k]);

// The three php DOES write through — its own handler hands back the element.
$ao = new ArrayObject(['a' => ['b' => 1]]);
$ao['a']['b'] = 9;
indModShow("ArrayObject     ", $ao['a']);
$ai = new ArrayIterator(['a' => ['b' => 1]]);
$ai['a']['b'] = 9;
indModShow("ArrayIterator   ", $ai['a']);
$wm = new WeakMap(); $wk = new stdClass(); $wm[$wk] = ['b' => 1];
$wm[$wk]['b'] = 9;
indModShow("WeakMap         ", $wm[$wk]);

// …but only while the native accessor is the one answering.
class IndModArrayObject extends ArrayObject {
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return parent::offsetGet($k); }
}
$sub = new IndModArrayObject(['a' => ['b' => 1]]);
$sub['a']['b'] = 9;
indModShow("overriding subclass", $sub['a']);
class IndModPlainSubclass extends ArrayObject {}
$plain = new IndModPlainSubclass(['a' => ['b' => 1]]);
$plain['a']['b'] = 9;
indModShow("plain subclass     ", $plain['a']);

// An offsetGet answering an OBJECT is not indirect at all: the handle is shared,
// so php stays silent and the write lands.
$g = new IndModBox([]);
$g['a'] = new ArrayObject(['b' => 1]);
$g['a']['b'] = 9;
indModShow("object element", $g->d['a']['b']);

// A by-reference offsetGet is php's other yes.
class IndModRefGet implements ArrayAccess {
    public $d = ['a' => ['b' => 1]];
    public function offsetExists($o): bool { return isset($this->d[$o]); }
    #[\ReturnTypeWillChange]
    public function &offsetGet($o) { return $this->d[$o]; }
    public function offsetSet($o, $v): void { $this->d[$o] = $v; }
    public function offsetUnset($o): void { unset($this->d[$o]); }
}
$rg = new IndModRefGet();
$rg['a']['b'] = 9;
indModShow("&offsetGet", $rg->d['a']);
?>
--EXPECT--
  [8] Indirect modification of overloaded element of IndModBox has no effect
nested  => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of IndModBox has no effect
append  => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of IndModBox has no effect
incr    => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of IndModBox has no effect
scalar  => 5
  [8] Indirect modification of overloaded element of IndModBox has no effect
unset   => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of IndModBox has no effect
ref     => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of IndModBox has no effect
foreach => array (
  'b' => 1,
)
read    => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of SplFixedArray has no effect
SplFixedArray    => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of SplStack has no effect
SplStack         => array (
  'b' => 1,
)
  [8] Indirect modification of overloaded element of SplObjectStorage has no effect
SplObjectStorage => array (
  'b' => 1,
)
ArrayObject      => array (
  'b' => 9,
)
ArrayIterator    => array (
  'b' => 9,
)
WeakMap          => array (
  'b' => 9,
)
  [8] Indirect modification of overloaded element of IndModArrayObject has no effect
overriding subclass => array (
  'b' => 1,
)
plain subclass      => array (
  'b' => 9,
)
object element => 9
&offsetGet => array (
  'b' => 9,
)
