--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionFunction/ReflectionMethod invoke() and invokeArgs() and ReflectionClass::newInstance() hand every argument's name on
--FILE--
<?php
declare(strict_types=1);
function rdnaRest(...$a) { var_dump($a); }
function rdnaFormal(int $a, $b = 5, ...$r) { var_dump($a, $b, $r); }
class RdnaM {
    function rest(...$a) { var_dump($a); }
    static function formal(int $p, ...$a) { var_dump($p, $a); }
}
class RdnaCtor { function __construct(...$a) { var_dump($a); } }
class RdnaPromo {
    function __construct(public int $p, public $q = 0) { var_dump($this->p, $this->q); }
}
$rdnaTry = function (callable $t) {
    try { $t(); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
};
$rest = new ReflectionFunction('rdnaRest');
$rest->invoke(1, x: 2);
$rest->invokeArgs([1, 'x' => 2]);
$formal = new ReflectionFunction('rdnaFormal');
$formal->invoke(1, b: 7, z: 9);
$formal->invoke(b: 1, a: "3");
$formal->invokeArgs(['b' => 1, 'a' => "3"]);
$rdnaTry(fn() => $formal->invoke(1, a: 2));
$rdnaTry(fn() => $formal->invokeArgs([1, 'a' => 2]));
$rdnaTry(fn() => $formal->invokeArgs(['x' => 2, 1]));
$rdnaTry(fn() => $formal->invokeArgs(['b' => 1]));
$rdnaTry(fn() => $formal->invokeArgs(['zz' => 1]));
$m = new ReflectionMethod('RdnaM', 'rest');
$m->invoke(new RdnaM, 1, x: 2);
$m->invokeArgs(new RdnaM, [1, 'x' => 2]);
$s = new ReflectionMethod('RdnaM', 'formal');
$s->invoke(null, p: 3, q: 4);
$s->invokeArgs(null, ['q' => 4, 'p' => "3"]);
$ctor = new ReflectionMethod('RdnaPromo', '__construct');
$rdnaTry(fn() => $ctor->invoke(new RdnaPromo(1), q: 1));
$rdnaTry(fn() => $ctor->invokeArgs(new RdnaPromo(1), ['zz' => 1, 'p' => 2]));
(new ReflectionClass('RdnaCtor'))->newInstance(1, x: 2);
(new ReflectionClass('RdnaCtor'))->newInstanceArgs([1, 'x' => 2]);
(new ReflectionClass('RdnaPromo'))->newInstance(q: 1, p: "4");
$rdnaTry(fn() => (new ReflectionClass('RdnaPromo'))->newInstance(1, p: 2));
--EXPECT--
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
int(1)
int(7)
array(1) {
  ["z"]=>
  int(9)
}
int(3)
int(1)
array(0) {
}
int(3)
int(1)
array(0) {
}
Error: Named parameter $a overwrites previous argument
Error: Named parameter $a overwrites previous argument
Error: Cannot use positional argument after named argument
ArgumentCountError: rdnaFormal(): Argument #1 ($a) not passed
ArgumentCountError: Too few arguments to function rdnaFormal(), 0 passed and at least 1 expected
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
int(3)
array(1) {
  ["q"]=>
  int(4)
}
int(3)
array(1) {
  ["q"]=>
  int(4)
}
int(1)
int(0)
ArgumentCountError: RdnaPromo::__construct(): Argument #1 ($p) not passed
int(1)
int(0)
Error: Unknown named parameter $zz
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
array(2) {
  [0]=>
  int(1)
  ["x"]=>
  int(2)
}
int(4)
int(1)
Error: Named parameter $p overwrites previous argument
