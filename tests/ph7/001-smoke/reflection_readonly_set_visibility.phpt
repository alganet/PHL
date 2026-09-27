--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly implies protected(set) only where the read side is public
--DESCRIPTION--
php models `readonly` as an implicit `protected(set)`, but only for a property
whose READ side is public: a `protected readonly` or `private readonly` one
already writes no wider than it reads, and an explicit `private(set)` beside
readonly IS the set visibility. This build added the bit to every readonly
property, so `private readonly int $v` reported modifiers 2180 where php reports
132 and named itself `private protected(set) readonly` — a set visibility WIDER
than the property's own read visibility, which php never reports.
--FILE--
<?php
class RoVis {
    private readonly int $v;
    protected readonly int $x;
    public readonly int $w;
    public private(set) readonly int $a;
    public protected(set) readonly int $b;
    protected private(set) readonly int $c;
    public private(set) int $d;
    public protected(set) int $e;
    public function __construct() {
        $this->v = 1; $this->x = 2; $this->w = 3;
        $this->a = 4; $this->b = 5; $this->c = 6;
        $this->d = 7; $this->e = 8;
    }
}
foreach ((new ReflectionClass('RoVis'))->getProperties() as $p) {
    printf("%s mods=%d [%s] readonly=%s private(set)=%s protected(set)=%s\n",
        $p->getName(), $p->getModifiers(),
        implode(' ', Reflection::getModifierNames($p->getModifiers())),
        var_export($p->isReadOnly(), true),
        var_export($p->isPrivateSet(), true),
        var_export($p->isProtectedSet(), true));
}
--EXPECT--
v mods=132 [private readonly] readonly=true private(set)=false protected(set)=false
x mods=130 [protected readonly] readonly=true private(set)=false protected(set)=false
w mods=2177 [public protected(set) readonly] readonly=true private(set)=false protected(set)=true
a mods=4257 [final public private(set) readonly] readonly=true private(set)=true protected(set)=false
b mods=2177 [public protected(set) readonly] readonly=true private(set)=false protected(set)=true
c mods=4258 [final protected private(set) readonly] readonly=true private(set)=true protected(set)=false
d mods=4129 [final public private(set)] readonly=false private(set)=true protected(set)=false
e mods=2049 [public protected(set)] readonly=false private(set)=false protected(set)=true
