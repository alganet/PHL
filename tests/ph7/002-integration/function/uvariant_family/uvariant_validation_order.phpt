--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The diff/intersect u-variant family validates its callbacks before any array argument
--FILE--
<?php
// php validates the trailing callback(s) BEFORE any of the arrays — including
// Argument #1 — and the value comparator (the lower position) ahead of the key
// comparator. Everything here is invalid on purpose; what matters is WHICH
// argument each engine names.
$probe = function (callable $f) {
    try { $f(); echo "no-throw\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
};
$probe(function () { array_diff_ukey(123, [1], 456); });
$probe(function () { array_udiff(123, [1], 456); });
$probe(function () { array_uintersect(123, [1], 456); });
$probe(function () { array_diff_uassoc(123, [1], 456); });
$probe(function () { array_udiff_uassoc(123, [1], 456, 789); });
$probe(function () { array_udiff_uassoc([1], [2], 456, "strcmp"); });
$probe(function () { array_udiff_uassoc([1], [2], "strcmp", 456); });
$probe(function () { array_intersect_ukey([1], "not-an-array", "not-a-function"); });
$probe(function () { array_uintersect_assoc([1], "not-an-array", 123); });
// The middle arrays are only checked once the callbacks pass.
$probe(function () { array_intersect_uassoc([1], "not-an-array", "strcmp"); });
// And a valid Argument #1 failure still names itself.
$probe(function () { array_udiff_assoc("nope", [1], "strcmp"); });
?>
--EXPECT--
TypeError: array_diff_ukey(): Argument #3 must be a valid callback, no array or string given
TypeError: array_udiff(): Argument #3 must be a valid callback, no array or string given
TypeError: array_uintersect(): Argument #3 must be a valid callback, no array or string given
TypeError: array_diff_uassoc(): Argument #3 must be a valid callback, no array or string given
TypeError: array_udiff_uassoc(): Argument #3 must be a valid callback, no array or string given
TypeError: array_udiff_uassoc(): Argument #3 must be a valid callback, no array or string given
TypeError: array_udiff_uassoc(): Argument #4 must be a valid callback, no array or string given
TypeError: array_intersect_ukey(): Argument #3 must be a valid callback, function "not-a-function" not found or invalid function name
TypeError: array_uintersect_assoc(): Argument #3 must be a valid callback, no array or string given
TypeError: array_intersect_uassoc(): Argument #2 must be of type array, string given
TypeError: array_udiff_assoc(): Argument #1 ($array) must be of type array, string given
