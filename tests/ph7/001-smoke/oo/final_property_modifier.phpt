--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.4 `final` on a property, in every spelling, and the modifier run under it
--DESCRIPTION--
php reads a member's modifiers as a SET, in any order and each at most once, and
`final` has been one of them for a PROPERTY since 8.4. The member loops here read
one modifier per branch and re-entered a keyword ladder, so `static final public
$p` fell out of it and `final` itself only ever led to a method or a constant --
every spelling of a final property was a parse error, a promoted `final public
int $p` included. The run is read in one place now, which is also what lets
`readonly`, `(set)` visibility, `static` and `abstract` appear in any order.
--FILE--
<?php
/* Every spelling php accepts, including the ones with no visibility at all. */
class FpmA { final public $a = 1; }
class FpmB { public final $b = 2; }
class FpmC { final $c = 3; }
class FpmD { final protected $d = 4; public function d() { return $this->d; } }
class FpmE { final public static $e = 5; }
class FpmF { static final public $f = 6; }
class FpmG { final public int $g = 7; }
class FpmH { final public readonly int $h; public function __construct() { $this->h = 8; } }
class FpmI { public readonly final int $i; public function __construct() { $this->i = 9; } }
class FpmJ { final public private(set) int $j = 10; }
class FpmK { final static int $k = 11; }
class FpmL { readonly final int $l; public function __construct() { $this->l = 12; } }
class FpmM { final public int $m { get => 13; } }

var_dump((new FpmA)->a, (new FpmB)->b, (new FpmC)->c, (new FpmD)->d());
var_dump(FpmE::$e, FpmF::$f, (new FpmG)->g, (new FpmH)->h, (new FpmI)->i);
var_dump((new FpmJ)->j, FpmK::$k, (new FpmL)->l, (new FpmM)->m);

/* A trait declares them too, and they compose like any other property. */
trait FpmT { final public int $t = 20; final public static $ts = 21; }
class FpmTC { use FpmT; }
var_dump((new FpmTC)->t, FpmTC::$ts);

/* A promoted constructor parameter takes `final` -- and, unlike a class body,
 * php lets it sit beside `private` there (modifiers 36). `final` alone promotes,
 * exactly as a lone `readonly` does. */
class FpmP { public function __construct(final public int $p) {} }
class FpmQ { public function __construct(final private int $q) {} }
class FpmR { public function __construct(final int $r) {} }
var_dump((new FpmP(30))->p, (new FpmR(31))->r);
var_dump((new ReflectionProperty('FpmQ', 'q'))->getModifiers());

/* Reflection: IS_FINAL is 32, isFinal() answers it, and the export prints the
 * word -- once, even when private(set) implies it too. */
$rp = new ReflectionProperty('FpmG', 'g');
var_dump($rp->isFinal(), $rp->getModifiers(), (string)$rp);
var_dump((new ReflectionProperty('FpmE', 'e'))->getModifiers());
var_dump((new ReflectionProperty('FpmD', 'd'))->getModifiers());
var_dump((new ReflectionProperty('FpmJ', 'j'))->getModifiers(), (string)(new ReflectionProperty('FpmJ', 'j')));
var_dump((new ReflectionProperty('FpmP', 'p'))->isFinal(), (new ReflectionProperty('FpmP', 'p'))->isPromoted());
$rn = new ReflectionProperty('FpmDflt', 'n');
var_dump($rn->isFinal(), $rn->getModifiers());

class FpmDflt { public int $n = 0; }   /* the non-final control */
/* A subclass that adds a DIFFERENT property is fine; the ban is on redeclaring. */
class FpmSub extends FpmG { public int $other = 40; }
var_dump((new FpmSub)->g, (new FpmSub)->other);

/* `final private function __construct` is php's one exemption from the
 * private-final warning: private stops `new`, final stops a subclass widening it
 * back, so the modifier does say something. Every other private method warns. */
class FpmSingleton { final private function __construct() {} public static function make() { return new self; } }
var_dump(FpmSingleton::make() instanceof FpmSingleton);

/* A non-public __destruct keeps the visibility it was DECLARED with. */
class FpmDtor { private function __destruct() {} public function keep() { return 1; } }
$rm = new ReflectionMethod('FpmDtor', '__destruct');
var_dump($rm->isPrivate(), $rm->isPublic(), $rm->getModifiers());
var_dump(strtok((string)$rm, "\n"));
var_dump(get_class_methods('FpmDtor'));
?>
--EXPECT--
int(1)
int(2)
int(3)
int(4)
int(5)
int(6)
int(7)
int(8)
int(9)
int(10)
int(11)
int(12)
int(13)
int(20)
int(21)
int(30)
int(31)
int(36)
bool(true)
int(33)
string(37) "Property [ final public int $g = 7 ]
"
int(49)
int(34)
int(4129)
string(51) "Property [ final public private(set) int $j = 10 ]
"
bool(true)
bool(true)
bool(false)
int(1)
int(7)
int(40)
bool(true)
bool(true)
bool(false)
int(4)
string(45) "Method [ <user> private method __destruct ] {"
array(1) {
  [0]=>
  string(4) "keep"
}
--CLEAN--
<?php
