--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An abstract method has an empty body, run by the doors that do not refuse it
--DESCRIPTION--
php compiles an abstract method (an interface's included) to an empty body.
Two doors run it instead of refusing: a LITERAL `X::__construct()` naming an
abstract or interface constructor -- which asks the class for its constructor
rather than looking a method up, so the abstract check never happens -- and a
closure from ReflectionMethod::getClosure(). The parameters still bind and
check, so arity and types refuse as for any call, and the call answers NULL.
Every other door refuses: a dynamic `self::$m()` name, a static call of any
other abstract method, and ReflectionMethod::invoke(), which refuses before it
looks at the receiver.
--FILE--
<?php
function acebTry($f) {
    try { var_dump($f()); }
    catch (Throwable $e) { echo get_class($e), ": ", preg_replace('/ in \S+ on line \d+| called in \S+ on line \d+/', '', $e->getMessage()), "\n"; }
}
function acebArg($x) { echo "arg $x\n"; return $x; }
abstract class AcebA {
    abstract function __construct(int $x, &$r = null);
    abstract function m(string $s);
    function f() {
        acebTry(fn() => self::__construct(acebArg(1)));
        acebTry(fn() => self::__construct());
        acebTry(fn() => self::__construct("zz"));
        acebTry(fn() => self::__CONSTRUCT(2, $q));
        acebTry(fn() => AcebA::__construct(3));
        acebTry(fn() => static::__construct(4));
        $c = self::__construct(...);
        acebTry(fn() => $c(5));
        acebTry(fn() => $c());
        $m = '__construct';
        acebTry(fn() => self::$m(6));
        foreach (['__construct', 'm'] as $n) {
            $r = new ReflectionMethod('AcebA', $n);
            acebTry(fn() => $r->invoke($this, 8));
            acebTry(fn() => $r->invoke(null));
            acebTry(fn() => $r->invokeArgs(new stdClass, []));
            acebTry(fn() => $r->getClosure($this)("9"));
            acebTry(fn() => $r->getClosure($this)([]));
        }
    }
}
class AcebB extends AcebA {
    function __construct(int $x = 0, &$r = null) { echo "B ctor $x\n"; }
    function m(string $s) {}
}
interface AcebI {
    function __construct(string $s);
    static function is();
}
class AcebC implements AcebI {
    function __construct(string $s = "") { echo "C ctor\n"; }
    static function is() {}
    function h() {
        acebTry(fn() => AcebI::__construct("s"));
        acebTry(fn() => AcebI::__construct([]));
        $c = AcebI::__construct(...);
        acebTry(fn() => $c("x"));
        acebTry(fn() => AcebI::is());
        acebTry(fn() => (new ReflectionMethod('AcebI', 'is'))->getClosure()());
        acebTry(fn() => (new ReflectionMethod('AcebI', 'is'))->invoke(null));
    }
}
(new AcebB)->f();
(new AcebC)->h();
acebTry(fn() => AcebA::__construct(1));
--EXPECT--
B ctor 0
arg 1
NULL
ArgumentCountError: Too few arguments to function AcebA::__construct(), 0 passed and at least 1 expected
TypeError: AcebA::__construct(): Argument #1 ($x) must be of type int, string given,
NULL
NULL
B ctor 4
NULL
NULL
ArgumentCountError: Too few arguments to function AcebA::__construct(), 0 passed and at least 1 expected
Error: Cannot call abstract method AcebA::__construct()
ReflectionException: Trying to invoke abstract method AcebA::__construct()
ReflectionException: Trying to invoke abstract method AcebA::__construct()
ReflectionException: Trying to invoke abstract method AcebA::__construct()
NULL
TypeError: AcebA::__construct(): Argument #1 ($x) must be of type int, array given,
ReflectionException: Trying to invoke abstract method AcebA::m()
ReflectionException: Trying to invoke abstract method AcebA::m()
ReflectionException: Trying to invoke abstract method AcebA::m()
NULL
TypeError: AcebA::m(): Argument #1 ($s) must be of type string, array given,
C ctor
NULL
TypeError: AcebI::__construct(): Argument #1 ($s) must be of type string, array given,
NULL
Error: Cannot call abstract method AcebI::is()
NULL
ReflectionException: Trying to invoke abstract method AcebI::is()
Error: Non-static method AcebA::__construct() cannot be called statically
