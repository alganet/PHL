--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter default is read in the class that DECLARED the method
--DESCRIPTION--
`self::K` in a parameter default means the class that declared the method -- for a
method composed from a TRAIT, the class that composed it, whoever calls it. A
default is a mini-program run in the callee's frame before the body starts, and the
call's self stack is not pushed until the body begins, so the trait rule had nothing
to walk from: `self` was left unresolved, read as a class of that name, and thrown as
`Class "self" not found` for a default php evaluates without a word. A method
declared by a CLASS already resolved, which is why only the trait shape failed.

Reflection asked the same question through two other doors and got neither right:
`getDefaultValue()` evaluated the default with no class marked at all (so even a
plain class's `self::K` was that Error), and `isDefaultValueConstant()` recognised
only a GLOBAL constant, so every class-constant default reported itself as no
constant -- where php answers true and names it the way the source spelled it.
--FILE--
<?php
interface PdcIface { const IK = 1; }
class PdcBase { const PK = 5; }
trait PdcTrait { const TK = 2; public function tm(int $a = self::TK, int $b = parent::PK) { return [$a, $b]; } }
class PdcC extends PdcBase implements PdcIface {
    use PdcTrait;
    const CK = 4;
    public function f(int $a = self::CK, int $b = parent::PK, int $c = PdcIface::IK, int $d = self::IK) { return [$a,$b,$c,$d]; }
    public static function g(int $a = self::CK) { return $a; }
}
function pdc_show($c, $m) {
    foreach ((new ReflectionMethod($c, $m))->getParameters() as $p) {
        printf("%s::%s $%-2s const=%d name=%s value=%s\n", $c, $m, $p->getName(),
            (int)$p->isDefaultValueConstant(),
            var_export($p->isDefaultValueConstant() ? $p->getDefaultValueConstantName() : null, true),
            var_export($p->getDefaultValue(), true));
    }
}
pdc_show('PdcC', 'f');
pdc_show('PdcC', 'g');
pdc_show('PdcC', 'tm');
/* An expression around a constant is not a constant default, and neither is a
 * class-name fetch -- php answers false for both while still evaluating them. */
class PdcX { const K = 3;
    public function h(int $a = self::K + 0, string $b = self::class, int $c = 7) { return [$a,$b,$c]; } }
pdc_show('PdcX', 'h');
/* The value is what the DECLARING class says, whoever asks. */
var_dump((new PdcC)->f(), PdcC::g(), (new PdcC)->tm());
?>
--EXPECT--
PdcC::f $a  const=1 name='self::CK' value=4
PdcC::f $b  const=1 name='parent::PK' value=5
PdcC::f $c  const=1 name='PdcIface::IK' value=1
PdcC::f $d  const=1 name='self::IK' value=1
PdcC::g $a  const=1 name='self::CK' value=4
PdcC::tm $a  const=1 name='self::TK' value=2
PdcC::tm $b  const=1 name='parent::PK' value=5
PdcX::h $a  const=0 name=NULL value=3
PdcX::h $b  const=0 name=NULL value='PdcX'
PdcX::h $c  const=0 name=NULL value=7
array(4) {
  [0]=>
  int(4)
  [1]=>
  int(5)
  [2]=>
  int(1)
  [3]=>
  int(1)
}
int(4)
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(5)
}
