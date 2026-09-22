--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a set hook's $value takes the PROPERTY's type, and the hook itself returns void
--FILE--
<?php
class C {
    public $seen;
    public int $num  { set { $this->seen = $value; } }
    public ?int $opt { set { $this->seen = $value; } }
    public int|string $uni { set { $this->seen = $value; } }
    public $any { set { $this->seen = $value; } }          // untyped: no coercion
    // An EXPLICIT parameter type keeps its own, wider, declaration.
    public int $wide { set(int|string $v) { $this->seen = $v; } }
}

$c = new C;
$c->num = "7";   var_dump($c->seen);
$c->num = 1.0;   var_dump($c->seen);
$c->opt = null;  var_dump($c->seen);
$c->uni = 1.0;   var_dump($c->seen);
$c->any = "7";   var_dump($c->seen);
$c->wide = "7";  var_dump($c->seen);

try { $c->num = "abc"; }
catch (TypeError $e) { echo $e->getMessage(), "\n"; }

// The `set => expr` shorthand is an assignment, not a return: its value still
// reaches the backing store.
class D {
    public int $twice { get { return $this->twice; } set => $value * 2; }
}
$d = new D;
$d->twice = "3";
var_dump($d->twice);

$h = (new ReflectionProperty('C', 'num'))->getHooks()['set'];
var_dump($h->getReturnType()?->getName(), (string)$h->getParameters()[0]->getType());
?>
--EXPECTF--
int(7)
int(1)
NULL
int(1)
string(1) "7"
string(1) "7"
C::$num::set(): Argument #1 ($value) must be of type int, string given, called in %s on line %d
int(6)
string(4) "void"
string(3) "int"
--CLEAN--
<?php
