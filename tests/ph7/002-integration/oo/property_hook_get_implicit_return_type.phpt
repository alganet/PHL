--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a get hook's return type IS the property type: it coerces, and rejects
--FILE--
<?php
class C {
    public int $num { get { return "5"; } }          // coerced, not passed through
    public ?string $str { get { return 5; } }
    public int $bad { get { return "x"; } }
    public array $arr { get { return 1; } }
    public int $none { get { } }                     // falls off the end
    public int $arrow { get => "7"; }
    public $untyped { get { return "x"; } }          // no type: nothing to enforce
}

$c = new C;
var_dump($c->num, $c->str, $c->arrow, $c->untyped);

foreach (['bad', 'arr', 'none'] as $p) {
    try { $c->$p; echo "no error\n"; }
    catch (TypeError $e) { echo $e->getMessage(), "\n"; }
}

// The type rides inheritance and the parent-hook call form.
abstract class A { abstract public int $v { get; } }
class B extends A {
    public int $v { get { return "11"; } }
}
class D extends B {
    public int $v { get { return parent::$v::get() + 1; } }
}
var_dump((new B)->v, (new D)->v);

// ...and reflection reports it exactly as php does.
$r = new ReflectionProperty('C', 'num');
var_dump($r->getHooks()['get']->getReturnType()?->getName());
?>
--EXPECT--
int(5)
string(1) "5"
int(7)
string(1) "x"
C::$bad::get(): Return value must be of type int, string returned
C::$arr::get(): Return value must be of type array, int returned
C::$none::get(): Return value must be of type int, none returned
int(11)
int(12)
string(3) "int"
--CLEAN--
<?php
