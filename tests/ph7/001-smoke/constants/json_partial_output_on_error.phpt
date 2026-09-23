--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_PARTIAL_OUTPUT_ON_ERROR substitutes and records instead of failing
--FILE--
<?php
echo "JSON_PARTIAL_OUTPUT_ON_ERROR=" . JSON_PARTIAL_OUTPUT_ON_ERROR . "\n";
// Inf/NaN substitute 0.
var_dump(json_encode(NAN, JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
var_dump(json_encode([INF, "ok"], JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
// An ill-formed string is null as a VALUE, "" as a KEY.
var_dump(json_encode("bad \xff utf8", JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
var_dump(json_encode(["\xff" => 1, "b" => 2], JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
// A resource is null; a value containing itself is null; a non-backed case is 0.
$f = fopen("php://memory", "r");
var_dump(json_encode($f, JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
fclose($f);
$a = [];
$a["self"] = &$a;
var_dump(json_encode($a, JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
unset($a["self"]);
enum JsonPartialCase { case A; }
var_dump(json_encode([1, JsonPartialCase::A, 2], JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
// $depth exceeded: the error is recorded but the encode CONTINUES, in full.
var_dump(json_encode([[1, 9], 8], JSON_PARTIAL_OUTPUT_ON_ERROR, 1), json_last_error());
// Several errors: the LAST one wins.
var_dump(json_encode([NAN, "\xff"], JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
var_dump(json_encode(["\xff", NAN], JSON_PARTIAL_OUTPUT_ON_ERROR), json_last_error());
// PARTIAL beats JSON_THROW_ON_ERROR: no exception, the substitute answers.
var_dump(json_encode(NAN, JSON_PARTIAL_OUTPUT_ON_ERROR | JSON_THROW_ON_ERROR), json_last_error());
?>
--EXPECT--
JSON_PARTIAL_OUTPUT_ON_ERROR=512
string(1) "0"
int(7)
string(8) "[0,"ok"]"
int(7)
string(4) "null"
int(5)
string(12) "{"":1,"b":2}"
int(5)
string(4) "null"
int(8)
string(13) "{"self":null}"
int(6)
string(7) "[1,0,2]"
int(11)
string(9) "[[1,9],8]"
int(1)
string(8) "[0,null]"
int(5)
string(8) "[null,0]"
int(7)
string(1) "0"
int(7)
--CLEAN--
<?php
