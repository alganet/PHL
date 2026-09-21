--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() converts a variable in place through every valid type name
--FILE--
<?php
$x = "123abc"; settype($x, "integer"); var_dump($x);
$x = "123abc"; settype($x, "int");     var_dump($x);
$x = "12.5x";  settype($x, "float");   var_dump($x);
$x = "7";      settype($x, "double");  var_dump($x);
$x = 42;       settype($x, "string");  var_dump($x);
$x = "0";      settype($x, "bool");    var_dump($x);
$x = "x";      settype($x, "boolean"); var_dump($x);
$x = 5;        settype($x, "array");   var_dump($x);
$x = null;     settype($x, "array");   var_dump($x);
$x = 5;        settype($x, "object");  var_dump($x);
$x = 5;        settype($x, "null");    var_dump($x);
$x = "5";      $ret = settype($x, "InTeGeR"); var_dump($ret, $x);
?>
--EXPECT--
int(123)
int(123)
float(12.5)
float(7)
string(2) "42"
bool(false)
bool(true)
array(1) {
  [0]=>
  int(5)
}
array(0) {
}
object(stdClass)#1 (1) {
  ["scalar"]=>
  int(5)
}
NULL
bool(true)
int(5)
