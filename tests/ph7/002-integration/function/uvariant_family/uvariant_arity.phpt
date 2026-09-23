--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The diff/intersect u-variant family counts its trailing callbacks into the minimum arity
--FILE--
<?php
// Arity: the callback(s) hide inside the variadic ...$rest, so the minimum is
// 2 for the single-callback members and 3 for the two-callback pair.
foreach (["array_diff_ukey", "array_intersect_ukey", "array_udiff_assoc",
          "array_uintersect_assoc", "array_intersect_uassoc"] as $fn) {
    try { $fn([1]); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
}
foreach (["array_udiff_uassoc", "array_uintersect_uassoc"] as $fn) {
    try { $fn([1], "strcmp"); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
array_diff_ukey() expects at least 2 arguments, 1 given
array_intersect_ukey() expects at least 2 arguments, 1 given
array_udiff_assoc() expects at least 2 arguments, 1 given
array_uintersect_assoc() expects at least 2 arguments, 1 given
array_intersect_uassoc() expects at least 2 arguments, 1 given
array_udiff_uassoc() expects at least 3 arguments, 2 given
array_uintersect_uassoc() expects at least 3 arguments, 2 given
