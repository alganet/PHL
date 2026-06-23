--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() magic hooks: __sleep / __serialize
--FILE--
<?php

class SzSleep { public $a=1; public $b=2; public $c=3; function __sleep(){ return ["c","a"]; } }
echo serialize(new SzSleep),"\n";
class SzSer { public $x=5; function __serialize():array{ return ["v"=>$this->x*2,"w"=>"k"]; } }
echo serialize(new SzSer),"\n";
?>
--EXPECT--
O:7:"SzSleep":2:{s:1:"c";i:3;s:1:"a";i:1;}
O:5:"SzSer":2:{s:1:"v";i:10;s:1:"w";s:1:"k";}
