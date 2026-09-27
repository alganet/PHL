--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
BcMath\Number's declared surface: a final readonly class with two VIRTUAL properties
--DESCRIPTION--
php builds `value` and `scale` out of its own struct rather than storing them,
which Reflection reports as `virtual` and which is why the object comparator
never sees them -- the compare handler decides every pair instead. The class is
final AND readonly. The `extends` refusal that pairing produces is in
002-integration/class/BcMathNumber, where an eval() fatal can be caught.
--FILE--
<?php
function bcnumber_surface_probe(): void {
    $c = new ReflectionClass('BcMath\Number');
    var_dump($c->getName(), $c->isFinal(), $c->isReadOnly(), $c->getModifiers(),
             $c->isInternal(), $c->getFileName(), $c->getInterfaceNames());
    foreach ($c->getProperties() as $p) {
        printf("%-6s %-7s modifiers=%d readonly=%s virtual=%s\n", $p->getName(),
            (string)$p->getType(), $p->getModifiers(),
            var_export($p->isReadOnly(), true), var_export($p->isVirtual(), true));
    }
    foreach ($c->getMethods() as $m) {
        $ps = [];
        foreach ($m->getParameters() as $p) {
            $s = ($p->getType() ? (string)$p->getType() : '') . ' $' . $p->getName();
            if ($p->isDefaultValueAvailable() && !$p->isDefaultValueConstant()) {
                $s .= ' = ' . var_export($p->getDefaultValue(), true);
            }
            $ps[] = $s;
        }
        printf("%s(%s): %s\n", $m->getName(), implode(', ', $ps),
            (string)($m->getReturnType() ?? '?'));
    }
    echo "## the extension owns fourteen functions and this one class\n";
    var_dump(extension_loaded('bcmath'), class_exists('BcMath\Number'),
             enum_exists('RoundingMode'));
}
bcnumber_surface_probe();
?>
--EXPECT--
string(13) "BcMath\Number"
bool(true)
bool(true)
int(65568)
bool(true)
bool(false)
array(1) {
  [0]=>
  string(10) "Stringable"
}
value  string  modifiers=2689 readonly=true virtual=true
scale  int     modifiers=2689 readonly=true virtual=true
__construct(string|int $num): ?
add(BcMath\Number|string|int $num, ?int $scale = NULL): BcMath\Number
sub(BcMath\Number|string|int $num, ?int $scale = NULL): BcMath\Number
mul(BcMath\Number|string|int $num, ?int $scale = NULL): BcMath\Number
div(BcMath\Number|string|int $num, ?int $scale = NULL): BcMath\Number
mod(BcMath\Number|string|int $num, ?int $scale = NULL): BcMath\Number
divmod(BcMath\Number|string|int $num, ?int $scale = NULL): array
powmod(BcMath\Number|string|int $exponent, BcMath\Number|string|int $modulus, ?int $scale = NULL): BcMath\Number
pow(BcMath\Number|string|int $exponent, ?int $scale = NULL): BcMath\Number
sqrt(?int $scale = NULL): BcMath\Number
floor(): BcMath\Number
ceil(): BcMath\Number
round(int $precision = 0, RoundingMode $mode): BcMath\Number
compare(BcMath\Number|string|int $num, ?int $scale = NULL): int
__toString(): string
__serialize(): array
__unserialize(array $data): void
## the extension owns fourteen functions and this one class
bool(true)
bool(true)
bool(true)
