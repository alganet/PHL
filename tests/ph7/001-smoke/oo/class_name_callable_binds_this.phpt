--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class-name callable is bound to the caller's compatible $this
--DESCRIPTION--
A callback naming a method through a class name ('A::m', ['A','m']) is bound to
the caller's $this when the running code's scope descends from that class and
$this is an instance of the scope, so static:: inside the method is the object's
class, for a static method and a non-static one alike. A static caller, a scope
outside the class's line, and the DIRECT $cb() spelling all call through the
named class. A Closure built from the callable keeps the binding.
--FILE--
<?php
class CnbX { static function sm() { return 'X:' . static::class; } }
class CnbA {
    function f() { return static::class . '/' . get_class($this); }
    static function sm() { return static::class; }
    function fromA() {
        return [call_user_func('CnbA::sm'), call_user_func(['CnbA', 'sm']),
            call_user_func('CnbC::sm'), call_user_func('CnbA::f')];
    }
}
class CnbC extends CnbA {
    function t() {
        return [
            call_user_func('CnbA::sm'), call_user_func(['CnbA', 'sm']),
            call_user_func('CnbA::f'), call_user_func(['CnbA', 'f']),
            call_user_func('CnbX::sm'), call_user_func('CnbC::sm'),
            array_map('CnbA::sm', [1]), call_user_func_array('CnbA::sm', []),
            forward_static_call('CnbA::sm'), forward_static_call(['CnbA', 'sm']),
            (function () { return call_user_func('CnbA::sm'); })(),
            (static fn() => call_user_func('CnbA::sm'))(),
            Closure::fromCallable('CnbA::sm')(), Closure::fromCallable(['CnbA', 'sm'])(),
            (new ReflectionMethod('CnbA', 'sm'))->invoke(null),
        ];
    }
    function direct() {
        $s = 'CnbA::sm';
        $p = ['CnbA', 'sm'];
        return [$s(), $p()];
    }
    static function st() { return [call_user_func('CnbA::sm'), call_user_func(['CnbA', 'sm'])]; }
}
class CnbD extends CnbC {}
class CnbY { function y() { return call_user_func('CnbA::sm'); } }
echo json_encode((new CnbC)->t()), "\n";
echo json_encode((new CnbD)->t()), "\n";
echo json_encode((new CnbD)->direct()), "\n";
echo json_encode([CnbC::st(), CnbD::st()]), "\n";
echo json_encode([(new CnbD)->fromA(), (new CnbC)->fromA()]), "\n";
echo json_encode((new CnbY)->y()), "\n";
?>
--EXPECT--
["CnbC","CnbC","CnbC\/CnbC","CnbC\/CnbC","X:CnbX","CnbC",["CnbC"],"CnbC","CnbC","CnbC","CnbC","CnbA","CnbC","CnbC","CnbA"]
["CnbD","CnbD","CnbD\/CnbD","CnbD\/CnbD","X:CnbX","CnbD",["CnbD"],"CnbD","CnbD","CnbD","CnbD","CnbA","CnbD","CnbD","CnbA"]
["CnbA","CnbA"]
[["CnbA","CnbA"],["CnbA","CnbA"]]
[["CnbD","CnbD","CnbC","CnbD\/CnbD"],["CnbC","CnbC","CnbC","CnbC\/CnbC"]]
"CnbA"
