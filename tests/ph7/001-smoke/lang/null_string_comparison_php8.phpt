--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8 comparison table: null loosely compared with a string compares as ""
--FILE--
<?php
// null vs string is a STRING comparison against "", not bool coercion
var_export(null == "0"); echo "\n";
var_export(null == ""); echo "\n";
var_export(null == "a"); echo "\n";
var_export(null == "00"); echo "\n";
var_export(null < "0"); echo "\n";
var_export(null > "0"); echo "\n";
echo null <=> "0", " ", "0" <=> null, " ", null <=> "", " ", null <=> "a", "\n";
var_export(null <= ""); echo "\n";
var_export(null >= ""); echo "\n";
// strict comparisons unaffected
var_export(null === ""); echo "\n";
var_export(null !== "0"); echo "\n";
// null vs non-strings keeps the bool-table behavior
var_export(null == 0); echo "\n";
var_export(null == 0.0); echo "\n";
var_export(null == false); echo "\n";
var_export(null == true); echo "\n";
var_export(null == []); echo "\n";
var_export(null < 1); echo "\n";
// bool vs string still compares as bool
var_export(false == "0"); echo "\n";
var_export(true == "a"); echo "\n";
// the same table drives in_array / array_search / array_keys
var_export(in_array(null, ["0", "x"])); echo "\n";
var_export(in_array(null, ["", "x"])); echo "\n";
var_export(in_array(null, ["0"], true)); echo "\n";
var_export(in_array(null, [0])); echo "\n";
var_export(in_array("", [null])); echo "\n";
var_export(array_search(null, ["0", "", null])); echo "\n";
var_export(array_search(null, ["0", "", null], true)); echo "\n";
echo json_encode(array_keys([null, "", "0", null, 0], null)), "\n";
echo json_encode(array_keys([null, "", "0", null, 0], null, true)), "\n";
// a needle must not be corrupted by the first comparison (regression:
// array_keys mutated its needle in place, breaking later elements)
echo json_encode(array_keys(["1", 1, "1", 1], "1", true)), "\n";
echo json_encode(array_keys(["a", "b", "a"], "a")), "\n";
// max/min follow the same ordering
var_export(max(null, "0")); echo "\n";
var_export(min("a", null)); echo "\n";
// regression: array_diff_assoc's strict compare must not corrupt the caller's
// live null elements (they used to come back as bool(false))
$nscA = ["x" => null, "y" => 1];
array_diff_assoc($nscA, ["x" => null]);
var_export($nscA["x"]); echo "\n";
?>
--EXPECT--
false
true
false
false
true
false
-1 1 0 -1
true
true
false
true
true
true
true
false
true
true
true
true
false
true
false
true
true
1
2
[0,1,3,4]
[0,3]
[0,2]
[0,2]
'0'
NULL
NULL
--CLEAN--
<?php
