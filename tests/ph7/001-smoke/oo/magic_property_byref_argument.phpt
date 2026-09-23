--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An accessor answers a by-REFERENCE argument where it is written, and the verdict at the call
--DESCRIPTION--
php calls `__get` (or a get hook) where the property is WRITTEN, whatever the parameter
turns out to be, and only then decides what a by-REFERENCE binding of the answer is:
`Indirect modification of overloaded property C::$p has no effect` for a magic property,
php's catchable `Indirect modification of C::$p is not allowed` for a HOOKED one — and
nothing at all for a by-VALUE parameter. PHL deferred the whole fetch to the call, so
__get ran after the later arguments' side effects and the notice was printed BEFORE the
accessor it describes; a nested `f($o->a[0])` never called __get at all and passed NULL;
and a hooked property was silently passed by value where php refuses.

An OBJECT is the exception php makes for itself: a handle is not "indirect", so no notice
is raised for one and a write through it really does land.
--FILE--
<?php
function mbaRef(&$x) { $x = 'W'; }
function mbaVal($x) { var_dump($x); }
function mbaProbe($label, $fn) {
    echo $label, ': ';
    /* One interpreter for the whole corpus: register and restore per probe. */
    set_error_handler(function ($n, $m) { echo '<', $m, '>'; return true; });
    try { $fn(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(); }
    restore_error_handler();
    echo "\n";
}
class MbaMagic {
    private $d = ['a' => [3, 1], 'o' => null];
    public function __construct() { $this->d['o'] = new stdClass; }
    public function __get($n) { echo '[get ', $n, ']'; return $this->d[$n] ?? null; }
    public function __set($n, $v) { echo '[set ', $n, ']'; $this->d[$n] = $v; }
    public function __isset($n) { return isset($this->d[$n]); }
}
class MbaHooked {
    public array $h { get { echo '[hook]'; return [3, 1]; } }
}

/* The accessor runs at the FETCH, and the notice belongs to the by-ref binding. */
mbaProbe('magic byref', function () { $o = new MbaMagic; mbaRef($o->a); });
mbaProbe('magic builtin', function () { $o = new MbaMagic; sort($o->a); });
mbaProbe('magic missing', function () { $o = new MbaMagic; mbaRef($o->zz); });
mbaProbe('magic nested elem', function () { $o = new MbaMagic; mbaRef($o->a[0]); });
mbaProbe('magic byval', function () { $o = new MbaMagic; mbaVal($o->a); });
mbaProbe('magic byval nested', function () { $o = new MbaMagic; mbaVal($o->a[0]); });
mbaProbe('magic byval builtin', function () { $o = new MbaMagic; echo count($o->a); });

/* An OBJECT answer is a handle: no notice, and the write lands on it. */
mbaProbe('magic object', function () {
    $o = new MbaMagic; mbaRef($o->o->deep); var_dump($o->o->deep);
});

/* A HOOKED property has no slot to alias, and `&get` does not exist here. */
mbaProbe('hook byref', function () { $o = new MbaHooked; mbaRef($o->h); });
mbaProbe('hook builtin', function () { $o = new MbaHooked; sort($o->h); });
mbaProbe('hook byval', function () { $o = new MbaHooked; mbaVal($o->h); });

/* The lookup contexts are unchanged: they never reach a by-ref binding. */
mbaProbe('isset', function () { $o = new MbaMagic; var_dump(isset($o->a), isset($o->zz)); });
mbaProbe('coalesce', function () { $o = new MbaMagic; var_dump($o->zz ?? 'def'); });
echo "end\n";
?>
--EXPECT--
magic byref: [get a]<Indirect modification of overloaded property MbaMagic::$a has no effect>
magic builtin: [get a]<Indirect modification of overloaded property MbaMagic::$a has no effect>
magic missing: [get zz]<Indirect modification of overloaded property MbaMagic::$zz has no effect>
magic nested elem: [get a]<Indirect modification of overloaded property MbaMagic::$a has no effect>
magic byval: [get a]array(2) {
  [0]=>
  int(3)
  [1]=>
  int(1)
}

magic byval nested: [get a]int(3)

magic byval builtin: [get a]2
magic object: [get o][get o]string(1) "W"

hook byref: [hook]Error: Indirect modification of MbaHooked::$h is not allowed
hook builtin: [hook]Error: Indirect modification of MbaHooked::$h is not allowed
hook byval: [hook]array(2) {
  [0]=>
  int(3)
  [1]=>
  int(1)
}

isset: bool(true)
bool(false)

coalesce: string(3) "def"

end
