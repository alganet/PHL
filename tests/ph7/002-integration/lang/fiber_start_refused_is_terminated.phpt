--TEST--
A fiber whose start() refused its arguments is started and terminated
--FILE--
<?php
// php binds a fiber's arguments INSIDE the fiber, so a refused one is the body
// throwing on its first switch: start() rethrows it and the fiber is started and
// terminated from then on -- getReturn() says it threw, and a second start() is
// refused. Every refusal shape reaches the same state: a declared type, too few
// arguments, an unknown or duplicated name, and a named hole.
function state(Fiber $f) {
    echo json_encode([$f->isStarted(), $f->isSuspended(), $f->isRunning(), $f->isTerminated()]), "\n";
}
function attempt(callable $c) {
    try { $c(); echo "no throw\n"; }
    catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
function typed(int $x) { echo "body ran\n"; }
function two(int $a, int $b) { echo "body ran\n"; }

$cases = [
    'type'     => [fn() => new Fiber('typed'), fn($f) => $f->start("abc")],
    'too few'  => [fn() => new Fiber('two'),   fn($f) => $f->start(1)],
    'unknown'  => [fn() => new Fiber('two'),   fn($f) => $f->start(1, c: 2)],
    'hole'     => [fn() => new Fiber('two'),   fn($f) => $f->start(b: 2)],
    'dup'      => [fn() => new Fiber('two'),   fn($f) => $f->start(1, a: 2)],
    'variadic' => [fn() => new Fiber(function (string ...$s) { echo "body ran\n"; }), fn($f) => $f->start([])],
];
foreach ($cases as $name => [$make, $start]) {
    echo "-- $name\n";
    $f = $make();
    attempt(fn() => $start($f));
    state($f);
    attempt(fn() => $f->start(1, 2));
    attempt(fn() => $f->getReturn());
    attempt(fn() => $f->resume());
    attempt(fn() => $f->throw(new Exception("x")));
    state($f);
}
echo "-- inside a fiber\n";
$outer = new Fiber(function () {
    $inner = new Fiber('typed');
    attempt(fn() => $inner->start("no"));
    state($inner);
    Fiber::suspend(1);
    echo "outer resumed\n";
});
var_dump($outer->start());
$outer->resume();
state($outer);
unset($f, $outer);
gc_collect_cycles();
echo "done\n";
--EXPECTF--
-- type
TypeError: typed(): Argument #1 ($x) must be of type int, string given
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- too few
ArgumentCountError: Too few arguments to function two(), 1 passed and exactly 2 expected
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- unknown
Error: Unknown named parameter $c
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- hole
ArgumentCountError: two(): Argument #1 ($a) not passed
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- dup
Error: Named parameter $a overwrites previous argument
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- variadic
TypeError: {closure:{closure:%s:23}:23}(): Argument #1 must be of type string, array given
[true,false,false,true]
FiberError: Cannot start a fiber that has already been started
FiberError: Cannot get fiber return value: The fiber threw an exception
FiberError: Cannot resume a fiber that is not suspended
FiberError: Cannot resume a fiber that is not suspended
[true,false,false,true]
-- inside a fiber
TypeError: typed(): Argument #1 ($x) must be of type int, string given
[true,false,false,true]
int(1)
outer resumed
[true,false,false,true]
done
