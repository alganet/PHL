--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionType family: named, union, intersection, DNF
--FILE--
<?php
function reflT(?int $a, string|float $b, int|null $c, Countable&Stringable $d, (Countable&Stringable)|array $e, mixed $m, ?ArrayAccess $o, $untyped): ?string { return null; }
class ReflTCls {
    public ?int $x;
    public string|int $y = 1;
    public $plain;
    public function v(): void {}
    public function nv(): never { throw new Exception('x'); }
    public function st(): static { return $this; }
}

$ps = (new ReflectionFunction('reflT'))->getParameters();
foreach ($ps as $p) {
    $t = $p->getType();
    if ($t === null) { echo $p->getName(), ": null\n"; continue; }
    echo $p->getName(), ': ', get_class($t), ' str=', (string)$t, ' null=', $t->allowsNull() ? 'y' : 'n';
    if ($t instanceof ReflectionNamedType) { echo ' name=', $t->getName(), ' builtin=', $t->isBuiltin() ? 'y' : 'n'; }
    if ($t instanceof ReflectionUnionType || $t instanceof ReflectionIntersectionType) {
        echo ' types=[';
        foreach ($t->getTypes() as $sub) {
            echo get_class($sub), ':', (string)$sub, ($sub instanceof ReflectionNamedType ? (':' . ($sub->isBuiltin() ? 'b' : 'c')) : ''), ',';
        }
        echo ']';
    }
    echo "\n";
}
$rt = (new ReflectionFunction('reflT'))->getReturnType();
echo 'ret: ', get_class($rt), ' str=', (string)$rt, ' name=', $rt->getName(), ' null=', $rt->allowsNull() ? 'y' : 'n', "\n";
$t = (new ReflectionProperty('ReflTCls', 'x'))->getType();
echo 'propx: ', get_class($t), ' str=', (string)$t, ' name=', $t->getName(), ' null=', $t->allowsNull() ? 'y' : 'n', "\n";
$ty = (new ReflectionProperty('ReflTCls', 'y'))->getType();
echo 'propy: ', get_class($ty), ' str=', (string)$ty, "\n";
echo 'plain: ', (new ReflectionProperty('ReflTCls', 'plain'))->getType() === null ? 'null' : 'typed', "\n";
$rv = (new ReflectionMethod('ReflTCls', 'v'))->getReturnType();
echo 'void: ', get_class($rv), ' str=', (string)$rv, ' builtin=', $rv->isBuiltin() ? 'y' : 'n', ' null=', $rv->allowsNull() ? 'y' : 'n', "\n";
echo 'never: ', (string)(new ReflectionMethod('ReflTCls', 'nv'))->getReturnType(), "\n";
$rs = (new ReflectionMethod('ReflTCls', 'st'))->getReturnType();
echo 'static: ', get_class($rs), ' str=', (string)$rs, ' builtin=', $rs->isBuiltin() ? 'y' : 'n', "\n";
?>
--EXPECT--
a: ReflectionNamedType str=?int null=y name=int builtin=y
b: ReflectionUnionType str=string|float null=n types=[ReflectionNamedType:string:b,ReflectionNamedType:float:b,]
c: ReflectionNamedType str=?int null=y name=int builtin=y
d: ReflectionIntersectionType str=Countable&Stringable null=n types=[ReflectionNamedType:Countable:c,ReflectionNamedType:Stringable:c,]
e: ReflectionUnionType str=(Countable&Stringable)|array null=n types=[ReflectionIntersectionType:Countable&Stringable,ReflectionNamedType:array:b,]
m: ReflectionNamedType str=mixed null=y name=mixed builtin=y
o: ReflectionNamedType str=?ArrayAccess null=y name=ArrayAccess builtin=n
untyped: null
ret: ReflectionNamedType str=?string name=string null=y
propx: ReflectionNamedType str=?int name=int null=y
propy: ReflectionUnionType str=string|int
plain: null
void: ReflectionNamedType str=void builtin=y null=n
never: never
static: ReflectionNamedType str=static builtin=n
