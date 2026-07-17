--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad length beyond php's HT_MAX_SIZE (2^30) is a catchable ValueError
--FILE--
<?php
function apgT($fn) { try { $r = $fn(); echo "OK:", count($r), "\n"; } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }
// beyond the cap: ValueError instead of a multi-minute fill loop + OOM
apgT(fn() => array_pad([1, 2], 2000000000, 0));
apgT(fn() => array_pad([1, 2], -2000000000, 0));
apgT(fn() => array_pad([1, 2], 1073741825, 0));
apgT(fn() => array_pad([1, 2], -1073741825, 0));
apgT(fn() => array_pad([1, 2], PHP_INT_MAX, 0));
apgT(fn() => array_pad([1, 2], PHP_INT_MIN, 0));
// the cap ignores the input array's size
apgT(fn() => array_pad(range(1, 20), 1073741825, 0));
// normal pads still work on both sides
apgT(fn() => array_pad([1, 2], 5, "p"));
apgT(fn() => array_pad([1, 2], -5, "p"));
apgT(fn() => array_pad([1, 2, 3], 2, "x"));
apgT(fn() => array_pad([], 100, 9));
?>
--EXPECT--
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
ValueError: array_pad(): Argument #2 ($length) must not exceed the maximum allowed array size
OK:5
OK:5
OK:3
OK:100
--CLEAN--
<?php
