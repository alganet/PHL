--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: nested arrays with '=>' key/value pairs
--FILE--
<?php
$a = [["a" => 1]];
echo $a[0]["a"], "\n";

$b = [["a" => 1], ["a" => 2]];
echo $b[0]["a"], "\n";
echo $b[1]["a"], "\n";

$c = [["a" => 1, "b" => 2], ["c" => 3]];
echo $c[0]["a"], "\n";
echo $c[0]["b"], "\n";
echo $c[1]["c"], "\n";

$d = [["x" => ["y" => 7]]];
echo $d[0]["x"]["y"], "\n";

$e = ["outer" => ["inner" => 42]];
echo $e["outer"]["inner"], "\n";

$f = ["k" => "v"];
$g = [$f["k"] => 99];
echo $g["v"], "\n";

$h = [["a" => 1]];
$i = array(array("a" => 1));
echo ($h == $i ? "equal" : "diff"), "\n";

$j = [function(){ return ["x" => 1]; }, function(){ return ["y" => 2]; }];
echo $j[0]()["x"], ",", $j[1]()["y"], "\n";

class ShortArrNestedKeyed_C { public $name = "hello"; }
$o = new ShortArrNestedKeyed_C;
$k = [$o->{"name"} => "v"];
echo $k["hello"], "\n";
?>
--EXPECT--
1
1
2
1
2
3
7
42
99
equal
1,2
v
--CLEAN--
<?php
unset($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$o);
