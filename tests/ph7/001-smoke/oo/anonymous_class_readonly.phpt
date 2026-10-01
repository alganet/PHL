--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A readonly ANONYMOUS class (PHP 8.3)
--DESCRIPTION--
`new readonly class(…) implements I { … }` is php 8.3's readonly anonymous
class. `readonly` is a context-sensitive ID, so it never reached the keyword
chain in the `new` operand and the `class` after it read as the `::class`
constant — `syntax error, unexpected token "class"`. The NAMED form
(`readonly class C {}`) has worked the whole time, which is what hid it.
It is the only modifier php admits there: `new final class {}` and
`new abstract class {}` stay parse errors in both engines, and a readonly class
still refuses a non-readonly base.
Found by linting the vendor trees: pest writes one in its parallel result
printer (Plugins/Parallel/Paratest/ResultPrinter.php).
The messages here are printed WITHOUT the class name on purpose — an anonymous
class's name is a recorded divergence, and pinning it would make this
test measure that instead.
--FILE--
<?php
interface AcrShape { public function get(): int; }

$acr_a = new readonly class(5) implements AcrShape {
    public function __construct(public int $v) {}
    public function get(): int { return $this->v; }
};
echo $acr_a->get(), "\n";
var_dump($acr_a instanceof AcrShape);
var_dump((new ReflectionClass($acr_a))->isReadOnly());
try {
    $acr_a->v = 9;
} catch (Error $e) {
    echo get_class($e), ': ', preg_replace('/property \S+::/', 'property <anon>::', $e->getMessage()), "\n";
}

// No constructor arguments, and no interface.
$acr_b = new readonly class {
    public int $z;
    public function __construct() { $this->z = 3; }
};
echo $acr_b->z, "\n";

// A plain anonymous class beside it is still writable.
$acr_c = new class(6) { public function __construct(public int $v) {} };
$acr_c->v = 7;
echo "plain anon: ", $acr_c->v, "\n";
var_dump((new ReflectionClass($acr_c))->isReadOnly());

// The named form, unchanged.
readonly class AcrNamed { public function __construct(public int $v) {} }
$acr_d = new AcrNamed(11);
echo $acr_d->v, "\n";
var_dump((new ReflectionClass('AcrNamed'))->isReadOnly());

// `readonly` is still an ordinary identifier everywhere else.
$readonly = 'still a variable';
echo $readonly, "\n";
echo stdClass::class, "\n";
?>
--EXPECT--
5
bool(true)
bool(true)
Error: Cannot modify readonly property <anon>::$v
3
plain anon: 7
bool(false)
11
bool(true)
still a variable
stdClass
--CLEAN--
<?php
unset($acr_a, $acr_b, $acr_c, $acr_d, $readonly);
