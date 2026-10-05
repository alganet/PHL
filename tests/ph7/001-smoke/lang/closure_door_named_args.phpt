--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::__invoke() and Closure::call() hand every argument's name on to the closure
--FILE--
<?php
class CdnaThis { public $v = 5; }
class CdnaMagic {
    function __call($n, $a) { echo "__call $n\n"; }
    static function __callStatic($n, $a) { echo "__callStatic $n\n"; }
}
$cdnaTry = function (callable $t) {
    try { $t(); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
};
$rest = function (...$a) { var_dump($a); };
$rest->__invoke(1, x: 2);
$rest->call(new CdnaThis, 1, x: 2);
$bound = function (...$a) { var_dump($this->v, $a); };
$bound->call(new CdnaThis, y: 3);
$formal = function ($p, $q = 0, ...$r) { var_dump($p, $q, $r); };
$formal->__invoke(q: 3, p: 1);
$formal->__invoke(1, zz: 9);
$formal->call(new CdnaThis, q: 7, p: 2);
$cdnaTry(fn() => $formal->__invoke(1, p: 2));
$cdnaTry(fn() => $formal->call(new CdnaThis, 1, p: 2));
$one = function ($p) { var_dump($p); };
$cdnaTry(fn() => $one->__invoke(nope: 2));
$cdnaTry(fn() => $one->call(new CdnaThis, nope: 2));
$cdnaTry(fn() => call_user_func([$one, '__invoke'], 1, p: 2));
call_user_func([$one, '__invoke'], p: 4);
$typed = function (int $x) { var_dump($x); };
$typed->__invoke(x: "6");
$typed->call(new CdnaThis, x: "7");
$tramp = Closure::fromCallable([new CdnaMagic, 'zz']);
$cdnaTry(fn() => $tramp->__invoke(1, x: 2));
$cdnaTry(fn() => $tramp->call(new CdnaMagic, 1, x: 2));
$tramp->__invoke(1, 2);
$stramp = Closure::fromCallable(['CdnaMagic', 'yy']);
$cdnaTry(fn() => $stramp->__invoke(x: 2));
?>
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
int(5)
array(1) {
  ["y"]=>
  int(3)
}
int(1)
int(3)
array(0) {
}
int(1)
int(0)
array(1) {
  ["zz"]=>
  int(9)
}
int(2)
int(7)
array(0) {
}
Error: Named parameter $p overwrites previous argument
Error: Named parameter $p overwrites previous argument
Error: Unknown named parameter $nope
Error: Unknown named parameter $nope
Error: Named parameter $p overwrites previous argument
int(4)
int(6)
int(7)
Error: Unknown named parameter $x
Error: Unknown named parameter $x
__call zz
Error: Unknown named parameter $x
--CLEAN--
<?php
