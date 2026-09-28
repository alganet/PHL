--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An uninitialized typed property answers an isset and vivifies for a dimension write
--DESCRIPTION--
Reading one is an Error, but three things are not reads. isset()/empty()/`??` ask whether it
HAS a value -- the instance path has answered that since typed properties landed, the STATIC
one never did, and `isset(C::$n)` threw where php answers false (the idiom every symfony/cache
adapter opens with). `??=` assigns over the unset slot. And a DIMENSION write AUTO-INITIALIZES
an array when the declared type has room for one -- php asks the type's mask, so `array`,
`?array`, `iterable`, `mixed` and any union with an array alternative say yes -- which is how
Doctrine's `public array $table;` gets filled. A read-MODIFY-write still reads, so it still
raises.
--FILE--
<?php
class UtpwHolder {
    public array $inst;
    public ?array $nul;
    public iterable $iter;
    public static array $stat;
    public int $scalar;
    public static int $sstat;
}
$utpw = new UtpwHolder;
$utpw->inst['n'] = 'v';
$utpw->nul[] = 'a';
$utpw->iter['k'] = 1;
$utpw->inst['deep']['er'] = 2;
UtpwHolder::$stat['s'] = 'z';
var_dump($utpw->inst, $utpw->nul, $utpw->iter, UtpwHolder::$stat);
var_dump(isset($utpw->scalar), empty($utpw->scalar), $utpw->scalar ?? 'd');
var_dump(isset(UtpwHolder::$sstat), empty(UtpwHolder::$sstat), UtpwHolder::$sstat ?? 'd');
class UtpwSub extends UtpwHolder {}
var_dump(isset(UtpwSub::$sstat));
$utpw->scalar ??= 7;
UtpwHolder::$sstat ??= 8;
var_dump($utpw->scalar, UtpwHolder::$sstat);
class UtpwPlain { public int $p; public static int $q; }
$utpwP = new UtpwPlain;
$utpwP->p = 3;
UtpwPlain::$q = 4;
var_dump($utpwP->p, UtpwPlain::$q);
?>
--EXPECT--
array(2) {
  ["n"]=>
  string(1) "v"
  ["deep"]=>
  array(1) {
    ["er"]=>
    int(2)
  }
}
array(1) {
  [0]=>
  string(1) "a"
}
array(1) {
  ["k"]=>
  int(1)
}
array(1) {
  ["s"]=>
  string(1) "z"
}
bool(false)
bool(true)
string(1) "d"
bool(false)
bool(true)
string(1) "d"
bool(false)
int(7)
int(8)
int(3)
int(4)
--CLEAN--
<?php
