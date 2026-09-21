--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$a[$k] =& $x binds by reference: vivifies an undefined base, keys "" / null correctly
--DESCRIPTION--
A by-ref store to an array element (`$a[$k] =& $x`) had two bugs. (1) HashmapInsertByRef
treated the empty-string key "" as an auto-index request, so `$a[""] =& $x` filed under 0 and
a second bind added a duplicate instead of rebinding; a null key threw a TypeError. It now
casts null->"" and binds/overwrites the "" element like the by-value path. (2) The `=&`
operator (precedence 12, not 18) never marked its target a write context, so a store to an
element of an UNDEFINED variable warned "Undefined variable" and dropped the bind instead of
auto-vivifying the container -- for every key, not just "". The target's base is now compiled
as a write target (EXPR_FLAG_LOAD_IDX_STORE|EXPR_FLAG_MEMBER_WRITE) exactly like a plain
assignment, without the compound-assign read (a `=&` rebinds without reading the target).
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

echo "== undefined base auto-vivifies, every key kind ==\n";
$x = 1; $a[0] =& $x; $x = 10; var_dump($a);
$y = 2; $b["k"] =& $y; var_dump($b);
$z = 3; $c[""] =& $z; var_dump($c);

echo "== null key: deprecate + bind under \"\" ==\n";
$w = 4; $d[null] =& $w; var_dump($d);

echo "== \"\" and null hit the SAME element; a re-bind overwrites ==\n";
$e = ["" => 99]; $v = 5; $e[""] =& $v; $v = 50; var_dump($e);

echo "== nested vivify ==\n";
$n = 6; $f[0][1] =& $n; var_dump($f);

echo "== writes through the bound element reach the source ==\n";
$src = 1; $g = []; $g["k"] =& $src; $g["k"] = 77; var_dump($src);

echo "== append and existing-array binds still work ==\n";
$p = 8; $h = [5]; $h[] =& $p; var_dump($h);
?>
--EXPECT--
== undefined base auto-vivifies, every key kind ==
array(1) {
  [0]=>
  &int(10)
}
array(1) {
  ["k"]=>
  &int(2)
}
array(1) {
  [""]=>
  &int(3)
}
== null key: deprecate + bind under "" ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(1) {
  [""]=>
  &int(4)
}
== "" and null hit the SAME element; a re-bind overwrites ==
array(1) {
  [""]=>
  &int(50)
}
== nested vivify ==
array(1) {
  [0]=>
  array(1) {
    [1]=>
    &int(6)
  }
}
== writes through the bound element reach the source ==
int(77)
== append and existing-array binds still work ==
array(2) {
  [0]=>
  int(5)
  [1]=>
  &int(8)
}
--CLEAN--
<?php
unset($x, $y, $z, $w, $v, $n, $src, $p, $a, $b, $c, $d, $e, $f, $g, $h);
