--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_multisort's argument language: orders and flags attach once each to the preceding array
--FILE--
<?php
// The argument list's little language: an int names an order or a flags value
// for the PRECEDING array, at most one of each; anything else is refused.
$probe = function (callable $f) {
    try { var_dump($f()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
};
$a = [2, 1];
$probe(function () use (&$a) { return array_multisort(SORT_ASC, $a); });
$probe(function () use (&$a) { return array_multisort($a, SORT_ASC, SORT_ASC); });
$probe(function () use (&$a) { return array_multisort($a, SORT_ASC, SORT_NUMERIC, SORT_FLAG_CASE); });
$probe(function () use (&$a) { return array_multisort($a, "4"); });
$probe(function () use (&$a) { return array_multisort($a, 4.0); });
$probe(function () use (&$a) { return array_multisort($a, true); });
$probe(function () use (&$a) { return array_multisort($a, null); });
$probe(function () use (&$a) { return array_multisort($a, 99); });
$probe(function () use (&$a) { return array_multisort($a, SORT_STRING | SORT_FLAG_CASE); });
$probe(function () { $x = [1, 2]; $y = [1]; return array_multisort($x, $y); });
$probe(function () { return array_multisort(5); });
// a literal array is prefer-ref: sorted as a temporary, silently
$probe(function () { return array_multisort([3, 1, 2]); });
?>
--EXPECT--
TypeError: array_multisort(): Argument #1 ($array) must be an array or a sort flag that has not already been specified
TypeError: array_multisort(): Argument #3 must be an array or a sort flag that has not already been specified
TypeError: array_multisort(): Argument #4 must be an array or a sort flag that has not already been specified
TypeError: array_multisort(): Argument #2 must be an array or a sort flag
TypeError: array_multisort(): Argument #2 must be an array or a sort flag
TypeError: array_multisort(): Argument #2 must be an array or a sort flag
TypeError: array_multisort(): Argument #2 must be an array or a sort flag
ValueError: array_multisort(): Argument #2 must be a valid sort flag
bool(true)
ValueError: Array sizes are inconsistent
TypeError: array_multisort(): Argument #1 ($array) must be an array or a sort flag that has not already been specified
bool(true)
