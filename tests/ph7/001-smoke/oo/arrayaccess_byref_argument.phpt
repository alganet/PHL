--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-REFERENCE argument of an overloaded element is php's notice, and lands nowhere
--DESCRIPTION--
php fetches a by-reference argument for WRITING, and what that means splits the way
its read_dimension handler does. A container that can only answer with a VALUE —
every userland ArrayAccess, the SPL classes on the standard handler, a subclass
that overrides offsetGet — gets `Indirect modification of overloaded element of C
has no effect` and the write is thrown away; PHL said nothing, and a nested one
(`f($o['a']['b'])`) reached the callee still sharing the container's own map, so the
write LANDED on the object php leaves untouched. A WRITABLE container (ArrayObject,
ArrayIterator, WeakMap) answers with the real element instead, and its write fetch
CREATES a missing key where PHL warned about a read and passed NULL.

The two halves need opposite treatment at the fetch: the accessor call is a side
effect php performs where the subscript is WRITTEN, so it runs there and its result
rides a prefetch carrier that only the by-ref verdict is still pending on, while a
writable container calls nothing and can wait for the callee like an array element.
--FILE--
<?php
class AbaBox implements ArrayAccess {
    public $d;
    public function __construct($d) { $this->d = $d; }
    public function offsetExists($o): bool { return isset($this->d[$o]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($o) { echo '[get ', var_export($o, true), ']'; return $this->d[$o] ?? null; }
    public function offsetSet($o, $v): void { echo '[set]'; $this->d[$o] = $v; }
    public function offsetUnset($o): void { unset($this->d[$o]); }
}
class AbaGetSub extends ArrayObject {
    public function offsetGet($k): mixed { return parent::offsetGet($k); }
}
function abaRef(&$x) { $x = 'W'; }
function abaVal($x) { var_dump($x); }
function abaProbe($label, $fn) {
    echo $label, ': ';
    /* One interpreter for the whole corpus: register and restore per probe. */
    set_error_handler(function ($n, $m) { echo '<', $m, '>'; return true; });
    try { $fn(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(); }
    restore_error_handler();
    echo "\n";
}

/* A container that answers with a VALUE: php's notice, and the store is untouched. */
abaProbe('userland hit', function () {
    $o = new AbaBox(['a' => [1, 2]]); abaRef($o['a']); echo json_encode($o->d);
});
abaProbe('userland miss', function () {
    $o = new AbaBox([]); abaRef($o['zz']); echo json_encode($o->d);
});
abaProbe('userland nested', function () {
    $o = new AbaBox(['a' => ['b' => 1]]); abaRef($o['a']['b']); echo json_encode($o->d);
});
abaProbe('userland builtin', function () {
    $o = new AbaBox(['a' => [3, 1]]); sort($o['a']); echo json_encode($o->d);
});
abaProbe('SplFixedArray', function () {
    $f = new SplFixedArray(1); $f[0] = [3, 1]; sort($f[0]); echo json_encode($f[0]);
});
abaProbe('SplObjectStorage', function () {
    $s = new SplObjectStorage; $k = new stdClass; $s[$k] = [3, 1]; sort($s[$k]); echo json_encode($s[$k]);
});
abaProbe('offsetGet override', function () {
    $g = new AbaGetSub(['a' => [3, 1]]); sort($g['a']); echo json_encode($g['a']);
});

/* By VALUE the same fetch is silent, and the accessor still answers it. */
abaProbe('userland byval', function () {
    $o = new AbaBox(['a' => [1, 2]]); abaVal($o['a']);
});
abaProbe('userland byval nested', function () {
    $o = new AbaBox(['a' => ['b' => 1]]); abaVal($o['a']['b']);
});
abaProbe('userland byval builtin', function () {
    $o = new AbaBox(['a' => [3, 1]]); echo count($o['a']);
});

/* A WRITABLE container hands back the real element, and its write fetch creates one. */
abaProbe('ArrayObject hit', function () {
    $ao = new ArrayObject(['a' => [3, 1]]); sort($ao['a']); echo json_encode($ao['a']);
});
abaProbe('ArrayObject miss', function () {
    $ao = new ArrayObject([]); abaRef($ao['zz']); echo json_encode($ao->getArrayCopy());
});
abaProbe('ArrayObject nested', function () {
    $ao = new ArrayObject(['a' => ['b' => 1]]); abaRef($ao['a']['b']); echo json_encode($ao['a']);
});
abaProbe('ArrayObject byval miss', function () {
    $ao = new ArrayObject([]); abaVal($ao['zz']);
});
abaProbe('WeakMap hit', function () {
    $w = new WeakMap; $k = new stdClass; $w[$k] = [3, 1]; sort($w[$k]); echo json_encode($w[$k]);
});
echo "end\n";
?>
--EXPECT--
userland hit: [get 'a']<Indirect modification of overloaded element of AbaBox has no effect>{"a":[1,2]}
userland miss: [get 'zz']<Indirect modification of overloaded element of AbaBox has no effect>[]
userland nested: [get 'a']<Indirect modification of overloaded element of AbaBox has no effect>{"a":{"b":1}}
userland builtin: [get 'a']<Indirect modification of overloaded element of AbaBox has no effect>{"a":[3,1]}
SplFixedArray: <Indirect modification of overloaded element of SplFixedArray has no effect>[3,1]
SplObjectStorage: <Indirect modification of overloaded element of SplObjectStorage has no effect>[3,1]
offsetGet override: <Indirect modification of overloaded element of AbaGetSub has no effect>[3,1]
userland byval: [get 'a']array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}

userland byval nested: [get 'a']int(1)

userland byval builtin: [get 'a']2
ArrayObject hit: [1,3]
ArrayObject miss: {"zz":"W"}
ArrayObject nested: {"b":"W"}
ArrayObject byval miss: <Undefined array key "zz">NULL

WeakMap hit: [1,3]
end
