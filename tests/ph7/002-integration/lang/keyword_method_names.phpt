--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reserved keywords are accepted as method names after ->, ?-> and ::
--FILE--
<?php
class C {
    public function throw($x){ return "T:$x"; }
    public function list($x){ return "L:$x"; }
    public function print($x){ return "P:$x"; }
    public function if($x){ return "I:$x"; }
    public function foreach($x){ return "F:$x"; }
    public function match($x){ return "M:$x"; }
    public function yield($x){ return "Y:$x"; }
    public function fn($x){ return "FN:$x"; }
    public function class($x){ return "CL:$x"; }
    public function echo($x){ return "E:$x"; }
    public function isset($x){ return "S:$x"; }
    public static function default($x){ return "D:$x"; }
}
$c = new C;
$n = null;
echo $c->throw("a"), $c->list("b"), $c->print("c"), $c->if("d"), $c->foreach("e"), "\n";
echo $c->match("f"), $c->yield("g"), $c->fn("h"), $c->class("i"), "\n";
echo $c->echo("j"), $c->isset("k"), "\n";
echo C::default("z"), "\n";
echo $c?->throw("ns"), "\n";
echo $n?->throw("x") === null ? "short-circuit\n" : "called\n";
?>
--EXPECT--
T:aL:bP:cI:dF:e
M:fY:gFN:hCL:i
E:jS:k
D:z
T:ns
short-circuit
--CLEAN--
<?php
