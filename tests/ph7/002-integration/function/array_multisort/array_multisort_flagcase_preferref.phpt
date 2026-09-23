--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_multisort's FLAG_CASE-masked order match, and prefer-ref through call_user_func_array
--FILE--
<?php
// php masks SORT_FLAG_CASE off for the ORDER match too, but reads the
// direction unmasked — SORT_DESC|SORT_FLAG_CASE consumes the order slot and
// quirkily sorts ASCENDING. And a prefer-ref builtin through
// call_user_func_array binds a value without a word.
$probe = function (callable $f) { try { var_dump($f()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } };
$a = [3, 1, 2];
$probe(function () use (&$a) { return array_multisort($a, SORT_DESC | SORT_FLAG_CASE); });
var_dump($a);
$b = [3, 1, 2];
$probe(function () use (&$b) { return array_multisort($b, SORT_DESC | SORT_FLAG_CASE, SORT_ASC); });
$c = [2, 1];
$probe(function () use (&$c, &$undefined) { return array_multisort($c, [4, 3], $undefined); });
var_dump(call_user_func_array("extract", [["q" => 1]]));
$lit = [[5, 2, 9]];
var_dump(call_user_func_array("array_multisort", $lit));
?>
--EXPECT--
bool(true)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
TypeError: array_multisort(): Argument #3 must be an array or a sort flag that has not already been specified
TypeError: array_multisort(): Argument #3 must be an array or a sort flag
int(1)
bool(true)
