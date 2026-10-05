--TEST--
A fiber runs an internal class method as its body
--FILE--
<?php
// A fiber whose body is an internal class's method runs it: an instance method, a
// static one named as a string or a pair, and a first-class callable taken from one.
function attempt(callable $c) {
    try { $r = $c(); echo "returned ", var_export($r, true), "\n"; }
    catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
$ao = new ArrayObject([3, 1, 2]);
$bodies = [
    'instance'  => [$ao, 'count'],
    'setter'    => [$ao, 'setFlags'],
    'static'    => 'DateTimeImmutable::createFromFormat',
    'pair'      => ['DateTimeImmutable', 'createFromFormat'],
    'fcc'       => $ao->getArrayCopy(...),
];
$args = [
    'instance' => [],
    'setter'   => [ArrayObject::ARRAY_AS_PROPS],
    'static'   => ['Y-m-d', '2026-01-02'],
    'pair'     => ['Y-m-d', '2026-01-02'],
    'fcc'      => [],
];
foreach ($bodies as $name => $body) {
    echo "-- $name\n";
    $f = new Fiber($body);
    attempt(fn() => $f->start(...$args[$name]));
    var_dump($f->isTerminated());
    $r = $f->getReturn();
    echo is_object($r) ? get_class($r) . " " . $r->format('Y-m-d') : var_export($r, true), "\n";
}
var_dump($ao->getFlags());
echo "-- refused\n";
$f = new Fiber([$ao, 'setFlags']);
attempt(fn() => $f->start());
attempt(fn() => $f->start("x"));
$f = new Fiber([$ao, 'setFlags']);
attempt(fn() => $f->start("x"));
echo "-- suspended from the callback of one\n";
$f = new Fiber([$ao, 'uasort']);
$f->start(function ($a, $b) { Fiber::suspend("$a<=>$b"); return $a <=> $b; });
while (!$f->isTerminated()) {
    $f->resume();
}
var_dump($f->getReturn(), $ao->getArrayCopy());
--EXPECT--
-- instance
returned NULL
bool(true)
3
-- setter
returned NULL
bool(true)
NULL
-- static
returned NULL
bool(true)
DateTimeImmutable 2026-01-02
-- pair
returned NULL
bool(true)
DateTimeImmutable 2026-01-02
-- fcc
returned NULL
bool(true)
array (
  0 => 3,
  1 => 1,
  2 => 2,
)
int(2)
-- refused
ArgumentCountError: ArrayObject::setFlags() expects exactly 1 argument, 0 given
FiberError: Cannot start a fiber that has already been started
TypeError: ArrayObject::setFlags(): Argument #1 ($flags) must be of type int, string given
-- suspended from the callback of one
bool(true)
array(3) {
  [1]=>
  int(1)
  [2]=>
  int(2)
  [0]=>
  int(3)
}
