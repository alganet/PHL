--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The six ways a Fiber::suspend() can be reached: straight from the body, through a closure, a first-class callable, an __invoke object, call_user_func() and an array_map() callback. A fiber runs on a native stack of its own, so the two that go through a C callback park that builtin's own C frame along with the PHP one and suspend like the rest.
--FILE--
<?php
function run(string $label, Closure $body): void
{
    $f = new Fiber($body);
    try {
        $f->start();
        if ($f->isSuspended()) {
            $f->resume('R');
        }
        echo $label, ": ", var_export($f->getReturn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

run('direct', function () { return Fiber::suspend('a'); });

run('closure', function () {
    $inner = function () { return Fiber::suspend('b'); };
    return $inner();
});

run('first-class callable', function () {
    $inner = function () { return Fiber::suspend('c'); };
    $g = $inner(...);
    return $g();
});

class Inv
{
    public function __invoke()
    {
        return Fiber::suspend('d');
    }
}
run('__invoke', function () { $o = new Inv(); return $o(); });

run('call_user_func', function () {
    return call_user_func(function () { return Fiber::suspend('e'); });
});

run('array_map', function () {
    $r = array_map(function ($x) { return Fiber::suspend('f'); }, [1]);
    return $r[0];
});
?>
--EXPECT--
direct: 'R'
closure: 'R'
first-class callable: 'R'
__invoke: 'R'
call_user_func: 'R'
array_map: 'R'
--CLEAN--
<?php
