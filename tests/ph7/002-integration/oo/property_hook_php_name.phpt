--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a property hook reports php's own name, never the engine's synthesized one
--FILE--
<?php
class C {
    public $p {
        get { return __FUNCTION__ . '|' . __METHOD__ . '|' . debug_backtrace()[0]['function']; }
        set { echo __METHOD__, "\n"; }
    }
    public $boom { get { throw new Exception('x'); } }
}

$c = new C;
echo $c->p, "\n";
$c->p = 1;

try { $c->boom; }
catch (Exception $e) {
    echo $e->getTraceAsString(), "\n";
    var_dump($e->getTrace()[0]['function']);
}

// Plain functions and methods are untouched by the rewrite.
function plain() { return __FUNCTION__ . '|' . __METHOD__; }
class D { public function m() { return __FUNCTION__ . '|' . __METHOD__; } }
echo plain(), "\n", (new D)->m(), "\n";
?>
--EXPECTF--
$p::get|C::$p::get|$p::get
C::$p::set
#0 %s(14): C->$boom::get()
#1 {main}
string(10) "$boom::get"
plain|plain
m|D::m
--CLEAN--
<?php
