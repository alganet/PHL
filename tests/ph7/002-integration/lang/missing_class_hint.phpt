--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a type hint naming a class that does not exist rejects every value
--FILE--
<?php
interface Iface {}
abstract class Abs {}
enum En { case A; }
class Impl extends Abs implements Iface {}
class Oth {}

function arg(Missing $c) { echo "arg ran\n"; }
function nullableArg(?Missing $c) { echo "nullableArg ran\n"; }
function variadic(Missing ...$c) { echo "variadic ran\n"; }
function ret(): Missing { return new Oth; }
class Holder {
    public Missing $p;
    public static Missing $s;
}

$show = function (callable $fn) {
    try { $fn(); }
    catch (TypeError $e) {
        $m = $e->getMessage();
        $cut = strpos($m, ', called in');
        echo $cut === false ? $m : substr($m, 0, $cut), "\n";
    }
};

// Nothing can be an instance of a class that does not exist.
$show(fn() => arg(new Oth));
$show(fn() => arg(5));
$show(fn() => arg(null));
$show(fn() => variadic(new Oth));
$show(fn() => ret());
$h = new Holder;
$show(function () use ($h) { $h->p = new Oth; });
$show(function () { Holder::$s = new Oth; });

// ...but a NULLABLE one still takes null.
$show(fn() => nullableArg(null));
$show(fn() => nullableArg(new Oth));

// The hint is only "missing" when nothing can produce it: an interface, an
// abstract class, an enum, a root-anchored name, a class declared LATER in the
// file and one an autoloader supplies all still resolve and are really checked.
echo "== still resolves ==\n";
function wantsIface(Iface $i) { echo "iface ok\n"; }
function wantsAbs(Abs $a) { echo "abs ok\n"; }
function wantsEnum(En $e) { echo "enum ok\n"; }
function wantsRoot(\Traversable $t) { echo "root ok\n"; }
function wantsLater(Later $l) { echo "later ok\n"; }
function wantsAuto(Auto1 $a) { echo "autoload ok\n"; }

$show(fn() => wantsIface(new Impl));
$show(fn() => wantsIface(new Oth));
$show(fn() => wantsAbs(new Impl));
$show(fn() => wantsEnum(En::A));
$show(fn() => wantsRoot(new ArrayIterator([])));
$show(fn() => wantsLater(new Later));
spl_autoload_register(function ($n) { if ($n === 'Auto1') { eval('class Auto1 {}'); } });
$show(fn() => wantsAuto(new Auto1));

class Later {}
?>
--EXPECT--
arg(): Argument #1 ($c) must be of type Missing, Oth given
arg(): Argument #1 ($c) must be of type Missing, int given
arg(): Argument #1 ($c) must be of type Missing, null given
variadic(): Argument #1 must be of type Missing, Oth given
ret(): Return value must be of type Missing, Oth returned
Cannot assign Oth to property Holder::$p of type Missing
Cannot assign Oth to property Holder::$s of type Missing
nullableArg ran
nullableArg(): Argument #1 ($c) must be of type ?Missing, Oth given
== still resolves ==
iface ok
wantsIface(): Argument #1 ($i) must be of type Iface, Oth given
abs ok
enum ok
root ok
later ok
autoload ok
--CLEAN--
<?php
