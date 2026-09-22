--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw from an instance-property DEFAULT aborts the `new` and the statement around it
--DESCRIPTION--
A property default is a bytecode container run by VmLocalExec while sharing the caller's VM
frame, so an enclosing try catches the throw IN PLACE and the nested exec returns
PH7_EXCEPTION. PH7_VmCreateClassInstanceFrame noted that in a local flag and still answered
SXRET_OK, so OP_NEW never learned: it finished the object, ran the CONSTRUCTOR, and the whole
statement resumed after the catch had already run. php aborts construction at the first bad
default -- no constructor, no object, no rest of the statement. The status is parked now
(VmBoundaryPark), which is exactly what the typed-property-default failure beside it already
did, so OP_NEW's existing construction-aborted route lands it.
--FILE--
<?php
class A { public $p = UNDEF_A; }

echo "== new A: the statement is abandoned ==\n";
$r = 'untouched';
try {
    $r = new A();
    echo "resumed\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
var_dump($r);

echo "== the constructor must not run ==\n";
class F {
    public $p = UNDEF_F;
    public function __construct() { echo "CTOR RAN\n"; }
}
try {
    new F();
    echo "resumed\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== a bad default in the MIDDLE stops the remaining ones ==\n";
class B { public $q = 1; public $p = UNDEF_B; public $r = 2; }
try {
    $b = new B();
    echo "resumed\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== inherited bad default ==\n";
class E extends A { public $z = 3; }
try {
    new E();
    echo "resumed\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== mid-expression: the surrounding call is abandoned too ==\n";
try {
    var_dump(strlen(new A()));
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== inside an array literal, twice ==\n";
try {
    $a = [new A(), new A()];
    echo "resumed with ", count($a), "\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== the loop around it keeps working ==\n";
for ($i = 0; $i < 3; $i++) {
    try {
        new A();
    } catch (Throwable $e) {
        echo "c$i:", $e->getMessage(), "\n";
    }
}

echo "== a subclass of Throwable is no different ==\n";
class Ex extends RuntimeException { public $p = UNDEF_EX; }
try {
    new Ex("m");
    echo "resumed\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== a GOOD default still constructs ==\n";
enum En: string { case X = 'x'; }
class G { public $p = En::X; public $n = 7; }
$g = new G();
echo $g->p->value, " ", $g->n, "\n";
$c = clone $g;
echo $c->p->value, " ", $c->n, "\n";
?>
--EXPECT--
== new A: the statement is abandoned ==
caught: Undefined constant "UNDEF_A"
string(9) "untouched"
== the constructor must not run ==
caught: Undefined constant "UNDEF_F"
== a bad default in the MIDDLE stops the remaining ones ==
caught: Undefined constant "UNDEF_B"
== inherited bad default ==
caught: Undefined constant "UNDEF_A"
== mid-expression: the surrounding call is abandoned too ==
caught: Undefined constant "UNDEF_A"
== inside an array literal, twice ==
caught: Undefined constant "UNDEF_A"
== the loop around it keeps working ==
c0:Undefined constant "UNDEF_A"
c1:Undefined constant "UNDEF_A"
c2:Undefined constant "UNDEF_A"
== a subclass of Throwable is no different ==
caught: Undefined constant "UNDEF_EX"
== a GOOD default still constructs ==
x 7
x 7
