--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::call binds $this+scope and invokes (zero, one and many args)
--FILE--
<?php
class Inc2Call {
  public $v;
  private $secret;
  function __construct($v){ $this->v = $v; $this->secret = "s$v"; }
}

/* zero extra args */
$f = function (){ return $this->v; };
echo $f->call(new Inc2Call(9)), "\n";             // 9

/* one arg */
$g = function ($x){ return $this->v + $x; };
echo $g->call(new Inc2Call(10), 5), "\n";         // 15

/* many args */
$h = function ($a, $b, $c){ return "$this->v:$a:$b:$c"; };
echo $h->call(new Inc2Call(1), "x", "y", "z"), "\n";  // 1:x:y:z

/* call() rescopes to $newThis's class -> private access works */
$p = function (){ return $this->secret; };
echo $p->call(new Inc2Call(4)), "\n";             // s4
?>
--EXPECT--
9
15
1:x:y:z
s4
--CLEAN--
<?php
