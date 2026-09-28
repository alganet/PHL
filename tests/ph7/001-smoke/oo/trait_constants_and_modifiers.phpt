--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait declares constants (PHP 8.2), final members and asymmetric set-visibility
--DESCRIPTION--
The trait body was a reduced copy of the class body: it refused `const` outright
("Traits cannot have constants"), and `final` and `private(set)` reached its
"Expecting method declaration" arm. The composition side had been ready all
along -- PH7_ClassUseTrait already copied hConst -- so what was missing was the
three modifier branches, which now mirror the class body's. Composing over a
name that is taken is a conflict only when the DEFINITION differs, and php's
notion of "differs" covers the whole declaration: value, visibility, `final` and
declared type alike. Reaching a trait constant THROUGH the trait is the one
access php refuses, and it refuses it from inside a trait method too.
--FILE--
<?php
/* PHP 8.2: a trait declares constants, and they compose into the using class
 * exactly as its properties do. */
trait TdcT {
    private const P = 'p';
    protected const Q = 'q';
    public const R = 'r';
    final const F = 'f';
    const int N = 5;
    public const string S = 'sss';

    final public function fin() { return 'fin'; }
    final protected static function finS() { return 'finS'; }
    public static function callFinS() { return static::finS(); }
    public function reach() { return [self::P, self::Q, self::R, self::F, static::N]; }
}
class TdcC { use TdcT; }
class TdcKid extends TdcC {}

var_dump(TdcC::R, TdcC::F, TdcC::N, TdcC::S);
var_dump((new TdcC)->reach());
var_dump((new TdcC)->fin(), TdcC::callFinS());
var_dump(TdcKid::R, (new TdcKid)->reach());

/* Reflection sees them as the composing class's own. */
$r = new ReflectionClass('TdcC');
var_dump($r->getConstant('R'), $r->getReflectionConstant('P')->isPrivate(),
         $r->getReflectionConstant('Q')->isProtected(),
         $r->getReflectionConstant('R')->getDeclaringClass()->getName());

/* A constant reached THROUGH the trait is the one thing php refuses -- from
 * inside a trait method as readily as from outside, and defined() says false. */
try { TdcT::R; } catch (Throwable $e) { var_dump(get_class($e), $e->getMessage()); }
var_dump(defined('TdcT::R'));
/* ...while Reflection over the trait itself still answers. */
var_dump((new ReflectionClass('TdcT'))->getConstant('R'));

/* Composing over a name that is taken is a conflict only when the DEFINITION
 * differs; two identical declarations compose fine, and a trait constant
 * overrides one inherited from a base class. */
trait TdcA { const K = 'x'; }
trait TdcB { const K = 'x'; }
class TdcSame { use TdcA, TdcB; }
class TdcOwn { use TdcA; const K = 'x'; }
class TdcBase { const K = 'base'; }
class TdcOver extends TdcBase { use TdcA; }
var_dump(TdcSame::K, TdcOwn::K, TdcOver::K, TdcBase::K);

/* PHP 8.4 asymmetric set-visibility, in a trait body too. */
trait TdcAsym {
    public private(set) int $v = 1;
    private(set) string $w = 'w';
    public function bump() { $this->v++; $this->w .= '!'; return [$this->v, $this->w]; }
}
class TdcAsymC { use TdcAsym; }
$a = new TdcAsymC;
var_dump($a->v, $a->w, $a->bump());
try { $a->v = 9; } catch (Throwable $e) { var_dump($e->getMessage()); }
?>
--EXPECT--
string(1) "r"
string(1) "f"
int(5)
string(3) "sss"
array(5) {
  [0]=>
  string(1) "p"
  [1]=>
  string(1) "q"
  [2]=>
  string(1) "r"
  [3]=>
  string(1) "f"
  [4]=>
  int(5)
}
string(3) "fin"
string(4) "finS"
string(1) "r"
array(5) {
  [0]=>
  string(1) "p"
  [1]=>
  string(1) "q"
  [2]=>
  string(1) "r"
  [3]=>
  string(1) "f"
  [4]=>
  int(5)
}
string(1) "r"
bool(true)
bool(true)
string(4) "TdcC"
string(5) "Error"
string(45) "Cannot access trait constant TdcT::R directly"
bool(false)
string(1) "r"
string(1) "x"
string(1) "x"
string(1) "x"
string(4) "base"
int(1)
string(1) "w"
array(2) {
  [0]=>
  int(2)
  [1]=>
  string(2) "w!"
}
string(66) "Cannot modify private(set) property TdcAsymC::$v from global scope"
--CLEAN--
<?php
