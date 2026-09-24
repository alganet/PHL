--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
intval()'s $base is read for a STRING value and no other, and its rules are C strtol()'s: a prefix per base, a scan that stops at the first foreign digit, and saturation
--FILE--
<?php
// The base is the second argument php's own manual leads with, and it decides
// the VALUE of every digit string that is not decimal.
var_dump(intval('ff', 16), intval('101', 2), intval('755', 8), intval('zz', 36));

// Base 16 skips an optional "0x"; base 2 skips an optional "0b" (strtol knows
// the first prefix and php strips the second itself before calling it).
var_dump(intval('0xff', 16), intval('0XfF', 16), intval('0b101', 2), intval('0B11', 2));

// Base 0 PICKS the base from that same prefix: 0x -> 16, 0b -> 2, a leading
// zero -> 8, anything else -> 10.
var_dump(intval('0x1f', 0), intval('0b101', 0), intval('0777', 0), intval('42', 0));

// The sign comes before the prefix, and only one of it.
var_dump(intval('-ff', 16), intval('+ff', 16), intval('-0b1', 2), intval('-0x1f', 16));
var_dump(intval('--ff', 16), intval('- ff', 16));

// Leading whitespace is skipped; the scan then stops at the first byte the base
// cannot spell, so a base-16 read of "12ag" is 0x12a and a bare prefix is 0.
var_dump(intval("  \t\n ff", 16), intval('12ag', 16), intval('0x', 16), intval('0x0x1', 16));
var_dump(intval('0b', 2), intval('-0b', 2), intval('0b12', 2), intval('00b1', 2));

// A base php's strtol cannot use answers 0 -- it is not an error.
var_dump(intval('1', 1), intval('1', 37), intval('1', -1), intval('ff', 16 + PHP_INT_MAX - PHP_INT_MAX));

// The result SATURATES at the int boundary rather than wrapping.
var_dump(intval('7fffffffffffffff', 16), intval('8000000000000000', 16));
var_dump(intval('-8000000000000000', 16), intval('-ffffffffffffffffff', 16));
var_dump(intval('zzzzzzzzzzzzz', 36));

// The base is read ONLY for a string. Everything else takes the ordinary int
// cast and ignores it -- an out-of-range base included.
var_dump(intval(255, 16), intval(255.9, 16), intval(true, 16), intval(null, 16));
var_dump(intval([1, 2], 16), intval([], 16), intval(7, 1), intval(7.0, 37));

// Base 10 is the ordinary cast too, so php's numeric-string rules apply there
// and strtol's do not: "0x1f" is 0 and "1e3" is 1000, while base 16 reads the
// same two as 0 and 0x1e3.
var_dump(intval('0x1f', 10), intval('1e3', 10), intval('1e3', 16), intval('1e3', 0));

// php hands strtol() the C string, so an embedded NUL ends it.
var_dump(intval("12\0 34", 16), intval("ff\0", 16), intval("\0ff", 16));

// An underscore is a php SOURCE separator, not a digit anywhere.
var_dump(intval('1_0', 16));

// php strips the "0b" by REBUILDING the string out of the sign it found and the
// bytes past the prefix, so strtol's whole prelude runs again over what is left:
// whitespace and a sign are accepted THERE.
var_dump(intval('0b-1', 2), intval("0b\t1", 2), intval('0b 1', 0), intval('0b-11', 0));
var_dump(intval('0b+1', 2), intval('0b  +11', 2), intval('0b--1', 2));
// A sign BEFORE the prefix is already spent, so a second one ends the scan.
var_dump(intval('-0b-1', 2), intval("-0b\t1", 2), intval('+0b-1', 2), intval('0bx', 2));

// Only base 2 (and the base-0 prefix that resolves to it) reads "0b" at all:
// base 16 spells the same bytes as digits.
var_dump(intval('0b0b1', 16), intval('0b101', 16), intval('0b1', 10));

// Past the "is it 10?" test the base is strtol's `int` parameter, which php
// reaches through a plain narrowing cast -- so 2^32+16 reads as base 16 and
// PHP_INT_MIN reads as base 0, while the early return still sees full width
// ("1e3" base 2^32+10 is strtol's 1, not the numeric string's 1000).
var_dump(intval('0x1A', 4294967312), intval('12ag', 4294967312), intval('ff', -4294967280));
var_dump(intval('010', PHP_INT_MIN), intval('0777', PHP_INT_MIN), intval('ff', PHP_INT_MAX));
var_dump(intval('1e3', 4294967306), intval('0b101', 4294967306));
?>
--EXPECT--
int(255)
int(5)
int(493)
int(1295)
int(255)
int(255)
int(5)
int(3)
int(31)
int(5)
int(511)
int(42)
int(-255)
int(255)
int(-1)
int(-31)
int(0)
int(0)
int(255)
int(298)
int(0)
int(0)
int(0)
int(0)
int(1)
int(0)
int(0)
int(0)
int(0)
int(0)
int(9223372036854775807)
int(9223372036854775807)
int(-9223372036854775808)
int(-9223372036854775808)
int(9223372036854775807)
int(255)
int(255)
int(1)
int(0)
int(1)
int(0)
int(7)
int(7)
int(0)
int(1000)
int(483)
int(1)
int(18)
int(255)
int(0)
int(1)
int(-1)
int(1)
int(1)
int(-3)
int(1)
int(3)
int(0)
int(0)
int(0)
int(0)
int(0)
int(45233)
int(45313)
int(0)
int(26)
int(298)
int(255)
int(8)
int(511)
int(0)
int(1)
int(0)
