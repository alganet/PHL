--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_filter()/preg_replace_callback_array() write php's &$count out-param
--FILE--
<?php
// Both declare an &$count php always fills in; PHL declared neither, so the
// caller's variable kept its previous value -- a silent wrong answer no arity
// or type check catches (the by-ref out-param family).
$n = 9;
var_dump(preg_filter("/a/", "b", "aaa", -1, $n), $n);
$n = 9;
var_dump(preg_filter("/q/", "b", "aaa", -1, $n), $n);
$n = 9;
var_dump(preg_filter("/a/", "b", ["aaa", "zzz", "ba"], -1, $n), $n);
$n = 9;
var_dump(preg_filter("/a/", "b", [], -1, $n), $n);

$n = 9;
var_dump(preg_replace_callback_array(["/a/" => fn($m) => "b"], "aaa", -1, $n), $n);
$n = 9;
var_dump(preg_replace_callback_array(
    ["/a/" => fn($m) => "b", "/z/" => fn($m) => "y"], ["az", "qq"], -1, $n), $n);

// php writes &$count only when the run SUCCEEDED: a pattern that fails to
// compile answers null and leaves the caller's variable alone. An ARRAY subject
// is not a failure -- it degrades to the empty array, count 0.
$n = 9;
var_dump(@preg_replace_callback_array(["/a/" => fn($m) => "X", "/[/" => fn($m) => "Y"],
    "az", -1, $n), $n);
$n = 9;
var_dump(@preg_replace_callback_array(["/[/" => fn($m) => "Y"], ["az"], -1, $n), $n);
// preg_filter always writes it, including on a bad pattern.
$n = 9;
var_dump(@preg_filter("/[/", "b", "az", -1, $n), $n);

// $flags reaches each callback's match array.
var_dump(preg_replace_callback_array(["/(a)(q)?(z)/" => fn($m) => var_export($m[2], true)],
    "az", -1, $n, PREG_UNMATCHED_AS_NULL), $n);
?>
--EXPECT--
string(3) "bbb"
int(3)
NULL
int(0)
array(2) {
  [0]=>
  string(3) "bbb"
  [2]=>
  string(2) "bb"
}
int(4)
array(0) {
}
int(0)
string(3) "bbb"
int(3)
array(2) {
  [0]=>
  string(2) "by"
  [1]=>
  string(2) "qq"
}
int(2)
NULL
int(9)
array(0) {
}
int(0)
NULL
int(0)
string(4) "NULL"
int(1)
--CLEAN--
<?php
