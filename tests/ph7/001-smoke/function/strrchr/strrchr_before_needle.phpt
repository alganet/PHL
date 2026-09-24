--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strrchr()'s $before_needle answers everything in FRONT of the last occurrence, delimiter excluded
--FILE--
<?php
// php 8.3's third argument. FALSE is the historical answer -- the last
// occurrence and everything after it -- and TRUE is its complement.
var_dump(strrchr('a/b/c', '/'), strrchr('a/b/c', '/', false), strrchr('a/b/c', '/', true));

// The delimiter belongs to neither half, so a leading or trailing one leaves an
// empty string on one side rather than false.
var_dump(strrchr('/abc', '/', true), strrchr('/abc', '/', false));
var_dump(strrchr('abc/', '/', true), strrchr('abc/', '/', false));

// Not found is FALSE in both directions, and so is a needle with no first
// character to look for.
var_dump(strrchr('abc', '/', true), strrchr('', '/', true), strrchr('abc', '', true));

// Only the needle's FIRST byte is looked for (php 8 dropped the "an int needle
// is an ordinal" reading), and the search is byte-exact -- a NUL is a byte like
// any other and a UTF-8 lead byte is not a character.
var_dump(strrchr('a/b/c', '/x', true), strrchr("ab\0cd", "\0", true));
var_dump(bin2hex(strrchr("\0a\0b", "\0", true)), strrchr('héllo', 'l', true));

// Repeated needles: the LAST one splits.
var_dump(strrchr('aaa', 'a', true), strrchr('abc', 'a', true), strrchr('abc', 'c', true));

// The argument is a bool, so the screen coerces whatever is written there.
var_dump(strrchr('abc', 'b', 1), strrchr('abc', 'b', 'x'), strrchr('abc', 'b', 0));
?>
--EXPECT--
string(2) "/c"
string(2) "/c"
string(3) "a/b"
string(0) ""
string(4) "/abc"
string(3) "abc"
string(1) "/"
bool(false)
bool(false)
bool(false)
string(3) "a/b"
string(2) "ab"
string(4) "0061"
string(4) "hél"
string(2) "aa"
string(0) ""
string(2) "ab"
string(1) "a"
string(1) "a"
string(2) "bc"
