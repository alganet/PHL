--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The callback-taking array_diff/intersect family validates the callback before the intermediary arrays
--FILE--
<?php
// Each call has BOTH a non-array middle argument AND an invalid callback. php
// names the callback argument, not the array, because it validates the callback
// (the last argument) first.
foreach (['array_udiff', 'array_uintersect', 'array_diff_uassoc'] as $fn) {
    try {
        $fn([1], 'not-an-array', 'not-a-function');
    } catch (TypeError $e) {
        echo $fn, ': ', $e->getMessage(), "\n";
    }
}
// And the valid calls still work.
var_dump(array_udiff([1, 2, 3], [2, 3], 'strcmp'));
?>
--EXPECT--
array_udiff: array_udiff(): Argument #3 must be a valid callback, function "not-a-function" not found or invalid function name
array_uintersect: array_uintersect(): Argument #3 must be a valid callback, function "not-a-function" not found or invalid function name
array_diff_uassoc: array_diff_uassoc(): Argument #3 must be a valid callback, function "not-a-function" not found or invalid function name
array(1) {
  [0]=>
  int(1)
}
--CLEAN--
<?php
