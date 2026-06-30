--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::bindTo / Closure::bind rebind $this on plain and method-FCC closures
--FILE--
<?php
class Inc2Bind {
  public $v;
  function __construct($v){ $this->v = $v; }
  function m(){ return $this->v; }
}

/* plain closure: bindTo injects $this into the body */
$f = function (){ return $this->v; };
$b = $f->bindTo(new Inc2Bind(7));
echo $b(), "\n";                                  // 7

/* with arguments */
$g = function ($x){ return $this->v + $x; };
$bg = $g->bindTo(new Inc2Bind(10));
echo $bg(5), "\n";                                // 15

/* Closure::bind() static form */
$h = Closure::bind($f, new Inc2Bind(99));
echo $h(), "\n";                                  // 99

/* rebinding a method-FCC closure to another instance */
$a = new Inc2Bind(1); $cb = $a->m(...);
echo $cb(), "\n";                                 // 1
$reb = $cb->bindTo(new Inc2Bind(2));
echo $reb(), "\n";                                // 2

/* independent: original binding survives rebinding to a new closure */
$b2 = $b->bindTo(new Inc2Bind(8));
echo $b2(), " ", $b(), "\n";                      // 8 7
?>
--EXPECT--
7
15
99
1
2
8 7
--CLEAN--
<?php
