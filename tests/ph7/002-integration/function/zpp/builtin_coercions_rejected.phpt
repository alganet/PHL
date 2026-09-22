--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deprecated builtin coercions (null / lossy float / invalid chars) are rejected (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php only DEPRECATES these coercions; PHL targets php's non-deprecated surface and
// rejects them with a TypeError/ValueError.
function tc(callable $c) { try { $c(); echo "NO-THROW\n"; } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }

tc(fn() => abs(null));
tc(fn() => addslashes(null));
tc(fn() => addcslashes(null, "x"));
tc(fn() => base_convert(null, 16, 10));
tc(fn() => str_contains(null, "x"));
tc(fn() => chr(1.5));
// array_key_exists() follows the ARRAY-OFFSET rules, so its lossy-float rejection is
// worded like $a[1.5]'s (and its NULL key is php-exact now -- see
// 002-integration/array/array_key_exists_offset_rules.phpt).
tc(fn() => array_key_exists(1.5, [1]));
tc(fn() => ord(null));
tc(fn() => ord(""));
tc(fn() => ord("abc"));
tc(fn() => base_convert("1g", 16, 10));
tc(fn() => hexdec("xyz"));
tc(fn() => array_fill(0, 2.7, "x"));
tc(fn() => array_chunk([1, 2], 1.5));
tc(fn() => range(null, 5));
tc(fn() => range(1, 5, null));

// valid uses still work
echo abs(-3), " ", chr(65), " ", ord("A"), " ", base_convert("ff", 16, 10), "\n";
?>
--EXPECT--
TypeError: abs(): Argument #1 ($num) must be of type int|float, null given
TypeError: addslashes(): Argument #1 ($string) must be of type string, null given
TypeError: addcslashes(): Argument #1 ($string) must be of type string, null given
TypeError: base_convert(): Argument #1 ($num) must be of type string, null given
TypeError: str_contains(): Argument #1 ($haystack) must be of type string, null given
TypeError: chr(): Argument #1 ($codepoint) must be of type int, float given
TypeError: Cannot access offset of type float on array
TypeError: ord(): Argument #1 ($character) must be of type string, null given
ValueError: ord(): Argument #1 ($character) must not be empty
ValueError: ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead
ValueError: Invalid characters passed for attempted conversion
ValueError: Invalid characters passed for attempted conversion
TypeError: Implicit conversion from float to int loses precision
TypeError: Implicit conversion from float to int loses precision
TypeError: range(): Argument #1 ($start) must be of type string|int|float, null given
TypeError: range(): Argument #3 ($step) must be of type int|float, null given
3 A 65 255
