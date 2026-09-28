--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A declared type's `self` belongs to the class that DECLARED it
--DESCRIPTION--
php resolves `self`/`parent` in a member's stored type TEXT at compile time, and
a TRAIT has no class to resolve them to -- so the text it stores for a trait
member keeps the keyword however many classes composed it, while a class's own
member carries the class name. Two engine answers were the wrong way round:
Reflection printed the keyword for everything, and `uninitialized(T)` plus the
assign TypeError printed the resolved class for everything, the trait's included.
`static` stays as written on both sides, and the type CHECK still resolves
against the composing class -- which is why the display scope had to be told
apart from the enforcement one.
--FILE--
<?php
class RtsRoot {}
trait RtsT {
    public ?self $tp = null;
    public function tm(self $a, ?parent $b): ?self { return null; }
}
class RtsA extends RtsRoot {
    use RtsT;
    public self $p;
    public ?parent $q = null;
    public function m(self $a, ?parent $b, self|int $c): ?self { return null; }
    public function r(): static { return $this; }
}
$c = new ReflectionClass('RtsA');
foreach ($c->getProperties() as $p) {
    printf("prop %s: %s\n", $p->getName(), (string) $p->getType());
}
foreach (['m', 'r', 'tm'] as $mn) {
    $m = new ReflectionMethod('RtsA', $mn);
    foreach ($m->getParameters() as $p) {
        printf("%s $%s: %s\n", $mn, $p->getName(), (string) $p->getType());
    }
    printf("%s ret: %s\n", $mn, (string) $m->getReturnType());
}
var_dump(new RtsA);
$o = new RtsA;
try { $o->p = 1; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $o->tp = 1; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $o->m(1, null, 1); } catch (\TypeError $e) { echo substr($e->getMessage(), 0, 48), "\n"; }
try { $o->tm(1, null); } catch (\TypeError $e) { echo substr($e->getMessage(), 0, 49), "\n"; }
?>
--EXPECTF--
prop p: RtsA
prop q: ?RtsRoot
prop tp: ?self
m $a: RtsA
m $b: ?RtsRoot
m $c: RtsA|int
m ret: ?RtsA
r ret: static
tm $a: self
tm $b: ?parent
tm ret: ?self
object(RtsA)#%d (2) {
  ["p"]=>
  uninitialized(RtsA)
  ["q"]=>
  NULL
  ["tp"]=>
  NULL
}
Cannot assign int to property RtsA::$p of type RtsA
Cannot assign int to property RtsA::$tp of type ?self
RtsA::m(): Argument #1 ($a) must be of type RtsA
RtsA::tm(): Argument #1 ($a) must be of type RtsA
--CLEAN--
<?php
