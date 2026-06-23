--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() arrays: lists, mixed keys, nesting
--FILE--
<?php

echo serialize([1,2,"x"=>3]),"\n";
echo serialize([[1],[2,3]]),"\n";
echo serialize([5=>"a","k"=>"b"]),"\n";
echo serialize([]),"\n";
echo serialize(["n"=>null,"b"=>true,"f"=>1.5,"a"=>[1,2]]),"\n";
?>
--EXPECT--
a:3:{i:0;i:1;i:1;i:2;s:1:"x";i:3;}
a:2:{i:0;a:1:{i:0;i:1;}i:1;a:2:{i:0;i:2;i:1;i:3;}}
a:2:{i:5;s:1:"a";s:1:"k";s:1:"b";}
a:0:{}
a:4:{s:1:"n";N;s:1:"b";b:1;s:1:"f";d:1.5;s:1:"a";a:2:{i:0;i:1;i:1;i:2;}}
