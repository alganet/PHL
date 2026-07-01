--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: trim-family and addcslashes/quotemeta honor `a..z` range charlists
--FILE--
<?php
// Malformed-range warnings are asserted verbatim in trim_range_invalid_warning.phpt;
// here we assert only the returned values, so silence warnings for a stable,
// cross-engine byte-identical stdout.
error_reporting(0);

// Valid, incrementing ranges expand to the full byte span.
$t = fn($got, $want, $tag) => print($got === $want ? "$tag OK\n" : "$tag FAIL got=[$got] want=[$want]\n");

$t(trim("HELLOworldHELLO", "A..Z"), "world",       "upper-range");
$t(trim("012ab210", "0..9"),        "ab",          "digit-range");
$t(ltrim("---abc", "!../"),         "abc",          "punct-range");
$t(rtrim("xyzHELLO", "A..Z"),       "xyz",          "rtrim-range");
$t(trim("a.b..c", "a..c"),          ".b..",         "range-plus-loose");
$t(trim("!@#abc#@!", "!..@"),       "abc",          "ascii-punct-range");
$t(trim("...a...b...", "a..b"),     "...a...b...",  "dot-not-in-range");

// addcslashes / quotemeta share the same mask builder.
$t(addcslashes("Hello World", "A..Z"), "\\Hello \\World", "addcslashes-range");
$t(quotemeta("1+1=2. (a)"),            "1\\+1=2\\. \\(a\\)", "quotemeta");

// Malformed ranges warn but still trim with the surrounding bytes as literals.
$t(trim("aXbz", "z..a"), "Xb", "reversed-range");
$t(trim("a.b.", "a.."),  "b",  "no-right-char");
$t(trim(".a.b", "..b"),  "a",  "no-left-char");
$t(addcslashes("aZ", "Z..A"), "a\\Z", "addcslashes-reversed");

// Regression guards: defaults and degenerate charlists unchanged.
$t(trim("  hi  "),        "hi",     "default-ws");
$t(trim("xxhixx", ""),    "xxhixx", "empty-charlist");
$t(trim("aaXbb", "ab"),   "X",      "plain-charlist");
?>
--EXPECT--
upper-range OK
digit-range OK
punct-range OK
rtrim-range OK
range-plus-loose OK
ascii-punct-range OK
dot-not-in-range OK
addcslashes-range OK
quotemeta OK
reversed-range OK
no-right-char OK
no-left-char OK
addcslashes-reversed OK
default-ws OK
empty-charlist OK
plain-charlist OK
--CLEAN--
<?php
