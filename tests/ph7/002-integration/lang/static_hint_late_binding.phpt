--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a `static` return type is the CALLED class, and the scope keywords fold like php
--FILE--
<?php
class Oth {}

class P {
    public function me(): static { return $this; }
    public function bad(): static { return new Oth; }
    public function fixed(): static { return new P; }
    public function nul(): ?static { return null; }
    public function nulBad(): ?static { return new Oth; }
    public function uni(): static|false { return false; }
    public function uniMe(): static|false { return $this; }
    // php folds a type keyword like every other keyword; PHL used to miss the
    // exact-match arm and then enforce nothing at all.
    public function upSelf(): SELF { return new Oth; }
    public function upStatic(): STATIC { return new Oth; }
    public function upParam(SELF $x) { echo "upParam ran\n"; }
}
class Q extends P {}

$show = function (callable $fn) {
    try { $fn(); echo "ok\n"; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
};

$p = new P;
$q = new Q;

// `static` follows the class the call was made THROUGH, so a base instance is
// no longer good enough once the method is reached on a subclass.
$show(fn() => var_dump(get_class($p->me())));
$show(fn() => var_dump(get_class($q->me())));
$show(fn() => var_dump(get_class($p->fixed())));
$show(fn() => $q->fixed());
$show(fn() => $p->bad());
$show(fn() => $q->bad());

// Nullable and union forms carry the same resolution.
$show(fn() => var_dump($p->nul()));
$show(fn() => $q->nulBad());
$show(fn() => var_dump($p->uni()));
$show(fn() => var_dump(get_class($q->uniMe())));

// An upper-cased keyword is the same keyword.
$show(fn() => $p->upSelf());
$show(fn() => $q->upStatic());
$show(fn() => $q->upParam(new Oth));
?>
--EXPECTF--
string(1) "P"
ok
string(1) "Q"
ok
string(1) "P"
ok
P::fixed(): Return value must be of type Q, P returned
P::bad(): Return value must be of type P, Oth returned
P::bad(): Return value must be of type Q, Oth returned
NULL
ok
P::nulBad(): Return value must be of type ?Q, Oth returned
bool(false)
ok
string(1) "Q"
ok
P::upSelf(): Return value must be of type P, Oth returned
P::upStatic(): Return value must be of type Q, Oth returned
P::upParam(): Argument #1 ($x) must be of type P, Oth given, called in %s on line %d
--CLEAN--
<?php
