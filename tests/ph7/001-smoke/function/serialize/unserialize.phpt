--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize(): round-trips (incl. exact float round-trip) and direct forms
--FILE--
<?php

foreach ([null,true,false,0,-42,42,3.14,1.5,1/3,0.123456789012345678,"hi","a\0b",[1,2,"k"=>3],[[1],[2,3]]] as $x)
    echo (unserialize(serialize($x)) === $x) ? "eq\n" : "NE\n";
class SzRound { public $a=1; protected $b=2; private $c=3; function g(){ return "$this->a,$this->b,$this->c"; } }
$o = unserialize(serialize(new SzRound));
echo get_class($o), ":", $o->g(), "\n";
echo unserialize("i:-42;"), "|", unserialize("d:1.5;"), "|", (unserialize("b:1;")?"T":"F"), "\n";
?>
--EXPECT--
eq
eq
eq
eq
eq
eq
eq
eq
eq
eq
eq
eq
eq
eq
SzRound:1,2,3
-42|1.5|T
