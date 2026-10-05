--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fiber body with no compiled coroutine body is called, and can still suspend
--DESCRIPTION--
php's fiber simply CALLS its callable on a stack of its own, so any callable is a
body. Three had no bytecode this engine could run as a coroutine and were refused
at start(), or died: a GENERATOR function (its body met `yield` outside a
Generator; php's fiber gets the Generator back as its return value), an internal
function, and a name php routes through `__call`/`__callStatic` -- including a
PRIVATE method, which php does not reach but routes to the handler instead. Each
is now dispatched the way an internal function calls a callback, from the fiber's
stack, so a callback array_map() reaches and a `__call` handler both suspend the
fiber, and named arguments and argument refusals are the callee's own.
--FILE--
<?php
function fnbGen($a) { echo "gen body $a\n"; yield $a; }
class FnbHost {
    private function priv() { return 'priv'; }
    public function __call($n, $a) { Fiber::suspend("in call:$n"); return "call:$n(" . implode(',', $a) . ")"; }
    public static function __callStatic($n, $a) { return "static:$n"; }
    public function gm() { yield 7; }
}
$o = new FnbHost;
$cases = [
    'generator'         => ['fnbGen', [5]],
    'generator-closure' => [function ($x) { yield $x; }, [3]],
    'generator-method'  => [[$o, 'gm'], []],
    'internal'          => ['strtoupper', ['ab']],
    'internal-fcc'      => [strlen(...), ['abcd']],
    'private-array'     => [[$o, 'priv'], [1, 2]],
    'missing-static'    => ['FnbHost::nope', []],
    'trampoline-fcc'    => [$o->zz(...), [9]],
    'array_map'         => ['array_map', [function ($v) { return Fiber::suspend($v) * 10; }, [1, 2]]],
    'bad-argument'      => ['strlen', [[]]],
    'too-few'           => ['str_repeat', ['x']],
];
foreach ($cases as $label => [$cb, $args]) {
    $f = new Fiber($cb);
    try {
        $r = $f->start(...$args);
        while (!$f->isTerminated()) {
            echo "$label suspended: ", var_export($r, true), "\n";
            $r = $f->resume($r);
        }
        $ret = $f->getReturn();
        echo "$label: ", $ret instanceof Generator ? 'Generator ' . $ret->current() : json_encode($ret), "\n";
    } catch (Throwable $e) {
        echo "$label: ", get_class($e), ': ', $e->getMessage(), "\n";
    }
}
$f = new Fiber('str_repeat');
$f->start(times: 3, string: 'ab');
var_dump($f->getReturn(), $f->isTerminated());
$f = new Fiber(function () { throw new LogicException('never reached'); yield 1; });
var_dump($f->start(), $f->getReturn() instanceof Generator);
echo "END\n";
?>
--EXPECT--
generator: gen body 5
Generator 5
generator-closure: Generator 3
generator-method: Generator 7
internal: "AB"
internal-fcc: 4
private-array suspended: 'in call:priv'
private-array: "call:priv(1,2)"
missing-static: "static:nope"
trampoline-fcc suspended: 'in call:zz'
trampoline-fcc: "call:zz(9)"
array_map suspended: 1
array_map suspended: 2
array_map: [10,20]
bad-argument: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
too-few: ArgumentCountError: str_repeat() expects exactly 2 arguments, 1 given
string(6) "ababab"
bool(true)
NULL
bool(true)
END
