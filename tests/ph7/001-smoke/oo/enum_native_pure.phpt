--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A PURE enum declared from C: no backing value, no from()/tryFrom(), and NOT final
--DESCRIPTION--
RoundingMode is the engine's first pure (unbacked) enum declared from C, where
PropertyHookType beside it is a backed one. A pure enum has no `value` property
and no from()/tryFrom() at all — those two ride BackedEnum — and it satisfies
UnitEnum only. The `final` answer is the part that is easy to get wrong: php
stamps ZEND_ACC_FINAL on a COMPILED enum, so `enum U {}` reports isFinal() true
and getModifiers() 32, while every enum php declares from C reports false and 0.
Neither can be extended either way — the refusal names the ENUM, not a final
class — so the flag is purely what Reflection answers.
--FILE--
<?php
function enum_native_pure_probe(): void {
    var_dump(RoundingMode::HalfEven->name);
    var_dump(RoundingMode::HalfEven === RoundingMode::HalfEven);
    var_dump(array_map(fn($c) => $c->name, RoundingMode::cases()));
    var_dump(RoundingMode::HalfEven instanceof UnitEnum,
             RoundingMode::HalfEven instanceof BackedEnum);
    var_dump(enum_exists('RoundingMode'), class_exists('RoundingMode'));

    echo "## a pure enum has no backing value and neither from() nor tryFrom()\n";
    var_dump(@RoundingMode::HalfEven->value);
    foreach (['from', 'tryFrom'] as $m) {
        var_dump(method_exists('RoundingMode', $m));
    }
    try { RoundingMode::from(1); } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }

    echo "## Reflection: internal, unbacked, and NOT final\n";
    $r = new ReflectionEnum('RoundingMode');
    var_dump($r->isEnum(), $r->isBacked(), $r->isInternal(), $r->getFileName());
    var_dump($r->isFinal(), $r->getModifiers());
    var_dump($r->getInterfaceNames());
    var_dump(array_map(fn($c) => $c->getName(), $r->getCases()));
    var_dump($r->getCase('HalfOdd')->getValue() === RoundingMode::HalfOdd);
    var_dump(array_map(fn($p) => [$p->getName(), (string)$p->getType(), $p->isReadOnly()],
                       (new ReflectionClass('RoundingMode'))->getProperties()));

    echo "## a COMPILED enum is final where the C-declared one is not\n";
    var_dump((new ReflectionEnum('EnumNativePureCompiled'))->isFinal(),
             (new ReflectionEnum('EnumNativePureCompiled'))->getModifiers());

    echo "## the refusals every enum makes\n";
    var_dump(serialize(RoundingMode::HalfOdd));
    var_dump(unserialize(serialize(RoundingMode::HalfOdd)) === RoundingMode::HalfOdd);
    try { new RoundingMode(); } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    try { $c = clone RoundingMode::HalfOdd; } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    $case = RoundingMode::HalfOdd;
    try { $case->name = 'x'; } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    echo match (RoundingMode::HalfOdd) {
        RoundingMode::HalfEven => "matched even\n",
        RoundingMode::HalfOdd  => "matched odd\n",
    };
}
enum EnumNativePureCompiled { case A; }
enum_native_pure_probe();
?>
--EXPECT--
string(8) "HalfEven"
bool(true)
array(8) {
  [0]=>
  string(16) "HalfAwayFromZero"
  [1]=>
  string(15) "HalfTowardsZero"
  [2]=>
  string(8) "HalfEven"
  [3]=>
  string(7) "HalfOdd"
  [4]=>
  string(11) "TowardsZero"
  [5]=>
  string(12) "AwayFromZero"
  [6]=>
  string(16) "NegativeInfinity"
  [7]=>
  string(16) "PositiveInfinity"
}
bool(true)
bool(false)
bool(true)
bool(true)
## a pure enum has no backing value and neither from() nor tryFrom()
NULL
bool(false)
bool(false)
Error: Call to undefined method RoundingMode::from()
## Reflection: internal, unbacked, and NOT final
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
int(0)
array(1) {
  [0]=>
  string(8) "UnitEnum"
}
array(8) {
  [0]=>
  string(16) "HalfAwayFromZero"
  [1]=>
  string(15) "HalfTowardsZero"
  [2]=>
  string(8) "HalfEven"
  [3]=>
  string(7) "HalfOdd"
  [4]=>
  string(11) "TowardsZero"
  [5]=>
  string(12) "AwayFromZero"
  [6]=>
  string(16) "NegativeInfinity"
  [7]=>
  string(16) "PositiveInfinity"
}
bool(true)
array(1) {
  [0]=>
  array(3) {
    [0]=>
    string(4) "name"
    [1]=>
    string(6) "string"
    [2]=>
    bool(true)
  }
}
## a COMPILED enum is final where the C-declared one is not
bool(true)
int(32)
## the refusals every enum makes
string(28) "E:20:"RoundingMode:HalfOdd";"
bool(true)
Error: Cannot instantiate enum RoundingMode
Error: Trying to clone an uncloneable object of class RoundingMode
Error: Cannot modify readonly property RoundingMode::$name
matched odd
