--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace_callback() takes array $pattern/$subject and honours $flags
--FILE--
<?php
// preg_replace() has handled array patterns and array subjects since forever;
// preg_replace_callback() stringified whichever one was an array, so an array
// subject replaced inside the literal text "Array" -- a silent wrong answer
// ("Arrby"), and an array pattern warned "Delimiter must not be alphanumeric".
$n = 9;
var_dump(preg_replace_callback("/a/", fn($m) => "b", ["az", "aa"], -1, $n), $n);
var_dump(preg_replace_callback(["/a/", "/z/"], fn($m) => "X", "az", -1, $n), $n);
var_dump(preg_replace_callback(["/a/", "/z/"], fn($m) => "X", ["az", "qq"], -1, $n), $n);

// $limit is per pattern per subject, &$count the accumulated total.
var_dump(preg_replace_callback("/(a)/", fn($m) => strtoupper($m[1]), ["az", "aa"], 1, $n), $n);

// Keys are preserved.
var_dump(preg_replace_callback("/a/", fn($m) => "b", ["x" => "aa", 7 => "a"]));

// A bad pattern: NULL for a scalar subject, an empty array for an array one --
// and &$count still carries what the earlier good patterns replaced.
$n = 9;
var_dump(@preg_replace_callback(["/a/", "/[/"], fn($m) => "X", "az", -1, $n), $n);
var_dump(@preg_replace_callback("/[/", fn($m) => "X", ["az"]));

// A zero-width match must emit the byte AT THE MATCH, not at the search start:
// a lookaround assertion matches AHEAD of it (the same fix preg_replace carries).
var_dump(preg_replace_callback("/(?<=[[:lower:]])(?=[[:upper:]])/", fn($m) => " ", "fooBarBaz"));

// $flags shapes the callback's match array exactly like preg_match()'s &$matches.
var_dump(preg_replace_callback("/(a)/", fn($m) => $m[0][1], "za", -1, $n, PREG_OFFSET_CAPTURE));
var_dump(preg_replace_callback("/(a)(q)?(z)/", fn($m) => var_export($m[2], true), "az",
    -1, $n, PREG_UNMATCHED_AS_NULL));

// A throwing callback still unwinds out of the array-subject loop.
try {
    preg_replace_callback("/a/", function ($m) { throw new Exception("boom"); }, ["aa", "ba"]);
} catch (Exception $e) {
    echo "caught ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(2) {
  [0]=>
  string(2) "bz"
  [1]=>
  string(2) "bb"
}
int(3)
string(2) "XX"
int(2)
array(2) {
  [0]=>
  string(2) "XX"
  [1]=>
  string(2) "qq"
}
int(2)
array(2) {
  [0]=>
  string(2) "Az"
  [1]=>
  string(2) "Aa"
}
int(2)
array(2) {
  ["x"]=>
  string(2) "bb"
  [7]=>
  string(1) "b"
}
NULL
int(1)
array(0) {
}
string(11) "foo Bar Baz"
string(2) "z1"
string(4) "NULL"
caught boom
--CLEAN--
<?php
