--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The magic-method declaration shapes php ACCEPTS still compile and still dispatch
--DESCRIPTION--
The other side of the compile-time declaration checks: everything the rules must
NOT reject. Each row here is a shape one of php's own conditions comes within a
hair of catching, so it is the set that would break first if the checks were
written a little too eagerly.

  * an OPTIONAL parameter counts toward the arity, so `__get($name = 'd')` is a
    valid one-argument declaration (`__get()` is not);
  * a VARIADIC tail counts for nothing, so `__clone(...$rest)` declares zero
    parameters and satisfies the argumentless row;
  * returning BY REFERENCE is not taking an argument by reference;
  * `__construct` and `__invoke` have no arity rule at all — php lets them take
    whatever they like, promoted constructor properties included;
  * `__clone` may declare `: void`, where its two neighbours `__construct` and
    `__destruct` may declare no return type at all;
  * `__set_state` and `__callStatic` are the two rows that must be static —
    every other row must not be — and they take one and two arguments;
  * and a name that merely LOOKS magic (`__notMagic`) has no rules at all.
--FILE--
<?php
class Shapes
{
    public array $bag = [];

    public function __construct(public int $id, private string $tag = "t", int ...$rest)
    {
        $this->bag = $rest;
    }

    public function __get($name = "defaulted")
    {
        return "get:$name";
    }

    public function __clone(...$rest): void
    {
        $this->bag = ["cloned"];
    }

    public function __invoke($a, $b, $c = 3)
    {
        return "invoke:" . ($a + $b + $c);
    }

    public static function __set_state($properties)
    {
        return "set_state:" . count($properties);
    }

    public static function __callStatic($name, $arguments)
    {
        return "callStatic:$name/" . count($arguments);
    }

    public function __notMagic($a, &$b, ...$c)
    {
        $b = "written";
        return "notMagic";
    }
}

class ByRefReturn
{
    private $store = ["k" => "v"];

    public function &__get($name)
    {
        return $this->store[$name];
    }
}

$o = new Shapes(7, "tag", 1, 2, 3);
echo $o->id, "\n";
echo implode(",", $o->bag), "\n";
echo $o->missing, "\n";
echo $o(1, 2), "\n";
echo Shapes::__set_state(["a" => 1, "b" => 2]), "\n";
echo Shapes::absent(1, 2, 3), "\n";

$clone = clone $o;
echo implode(",", $clone->bag), "\n";

$byRef = "before";
echo $o->__notMagic("x", $byRef), " ", $byRef, "\n";

$r = new ByRefReturn();
echo $r->k, "\n";
?>
--EXPECT--
7
1,2,3
get:missing
invoke:6
set_state:2
callStatic:absent/3
cloned
notMagic written
v
