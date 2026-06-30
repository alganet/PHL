--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::bindTo scope grants private/protected access to the bound object
--FILE--
<?php
class Inc2Scope {
  private $secret = "hidden";
  protected $prot = "guarded";
}

/* explicit class-name scope -> private + protected accessible */
$reader = function (){ return $this->secret . "/" . $this->prot; };
$bound = $reader->bindTo(new Inc2Scope(), Inc2Scope::class);
echo $bound(), "\n";                              // hidden/guarded

/* scope given as an object also works */
$r2 = function (){ return $this->secret; };
$b2 = $r2->bindTo(new Inc2Scope(), new Inc2Scope());
echo $b2(), "\n";                                 // hidden

/* Closure::call() rescopes to the bound object's class -> private accessible */
$r3 = function (){ return $this->secret; };
echo $r3->call(new Inc2Scope()), "\n";            // hidden
?>
--EXPECT--
hidden/guarded
hidden
hidden
--CLEAN--
<?php
