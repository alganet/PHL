--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionEnum / ReflectionEnumUnitCase / ReflectionEnumBackedCase, declared from C
--FILE--
<?php
enum REnumSuit: string {
    case Hearts = 'H';
    case Spades = 'S';
    const Wild = self::Spades;
    public function label(): string { return $this->name; }
}
enum REnumPure { case Alpha; case Beta; }
enum REnumNum: int { case One = 1; }
class REnumPlain { const K = 1; }

$r = new ReflectionEnum('REnumSuit');
echo $r->getName(), ' ', var_export($r->isBacked(), true), ' ', (string)$r->getBackingType(),
    ' ', get_class($r->getBackingType()), ' ',
    var_export($r->getBackingType()->isBuiltin(), true), "\n";
echo var_export($r->hasCase('Hearts'), true), ' ', var_export($r->hasCase('hearts'), true),
    ' ', var_export($r->hasCase('Wild'), true), ' ', var_export($r->hasCase('Nope'), true), "\n";
foreach ($r->getCases() as $c) {
    echo get_class($c), ' ', $c->getName(), ' ', $c->class, ' ', $c->getBackingValue(),
        ' ', $c->getValue()->value, ' ', $c->getEnum()->getName(),
        ' ', var_export($c->isEnumCase(), true), "\n";
}
echo count($r->getReflectionConstants()), ' ', count($r->getCases()), ' ',
    var_export($r->isEnum(), true), "\n";

$p = new ReflectionEnum('REnumPure');
echo var_export($p->isBacked(), true), ' ', var_export($p->getBackingType(), true), ' ',
    get_class($p->getCase('Alpha')), ' ', $p->getCase('Alpha')->getValue()->name, "\n";
echo (string)(new ReflectionEnum('REnumNum'))->getBackingType(), "\n";

/* The messages php words, and the two ways getCase() can fail */
$bad = [
    fn() => new ReflectionEnum('REnumPlain'),
    fn() => new ReflectionEnum('REnumGhost'),
    fn() => $r->getCase('Wild'),
    fn() => $r->getCase('Ghost'),
    fn() => new ReflectionEnumUnitCase('REnumSuit', 'Ghost'),
    fn() => new ReflectionEnumUnitCase('REnumSuit', 'Wild'),
    fn() => new ReflectionEnumUnitCase('REnumPlain', 'K'),
    fn() => new ReflectionEnumBackedCase('REnumPure', 'Alpha'),
];
foreach ($bad as $f) {
    try { $f(); echo "no throw\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* Engine state: php refuses to copy or serialize any of them */
foreach ([$r, new ReflectionEnumUnitCase('REnumPure', 'Alpha'),
          new ReflectionEnumBackedCase('REnumSuit', 'Hearts')] as $o) {
    try { $x = clone $o; echo "cloned\n"; } catch (Throwable $e) { echo $e->getMessage(), "\n"; }
    try { serialize($o); echo "serialized\n"; } catch (Throwable $e) { echo $e->getMessage(), "\n"; }
}

/* The class surface itself */
foreach (['ReflectionEnum', 'ReflectionEnumUnitCase', 'ReflectionEnumBackedCase'] as $c) {
    $rc = new ReflectionClass($c);
    echo $c, ' extends ', $rc->getParentClass()->getName(), ' final=',
        var_export($rc->isFinal(), true), "\n";
}
$m = new ReflectionMethod('ReflectionEnum', 'getCase');
echo $m->getNumberOfParameters(), ' ', (string)$m->getParameters()[0]->getType(), ' ',
    $m->getParameters()[0]->getName(), ' ', (string)$m->getReturnType(), "\n";
echo (string)(new ReflectionMethod('ReflectionEnum', 'getBackingType'))->getReturnType(), ' ',
    (string)(new ReflectionMethod('ReflectionEnumBackedCase', 'getBackingValue'))->getReturnType(), "\n";
try { $r->hasCase(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo new ReflectionEnumUnitCase('REnumSuit', 'Hearts');
--EXPECT--
REnumSuit true string ReflectionNamedType true
true false false false
ReflectionEnumBackedCase Hearts REnumSuit H H REnumSuit true
ReflectionEnumBackedCase Spades REnumSuit S S REnumSuit true
3 2 true
false NULL ReflectionEnumUnitCase Alpha
int
ReflectionException: Class "REnumPlain" is not an enum
ReflectionException: Class "REnumGhost" does not exist
ReflectionException: REnumSuit::Wild is not a case
ReflectionException: Case REnumSuit::Ghost does not exist
ReflectionException: Constant REnumSuit::Ghost does not exist
ReflectionException: Constant REnumSuit::Wild is not a case
ReflectionException: Constant REnumPlain::K is not a case
ReflectionException: Enum case REnumPure::Alpha is not a backed case
Trying to clone an uncloneable object of class ReflectionEnum
Serialization of 'ReflectionEnum' is not allowed
Trying to clone an uncloneable object of class ReflectionEnumUnitCase
Serialization of 'ReflectionEnumUnitCase' is not allowed
Trying to clone an uncloneable object of class ReflectionEnumBackedCase
Serialization of 'ReflectionEnumBackedCase' is not allowed
ReflectionEnum extends ReflectionClass final=false
ReflectionEnumUnitCase extends ReflectionClassConstant final=false
ReflectionEnumBackedCase extends ReflectionEnumUnitCase final=false
1 string name ReflectionEnumUnitCase
?ReflectionNamedType string|int
ArgumentCountError: ReflectionEnum::hasCase() expects exactly 1 argument, 0 given
Constant [ public REnumSuit Hearts ] { Object }
