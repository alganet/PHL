--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_trim decodes before it compares, and screens its $encoding
--FILE--
<?php
// php trims CHARACTERS, not bytes: an ill-formed run is one character that
// compares equal to every other ill-formed run, so a single bad byte in
// $characters strips them all. And what it KEEPS is decoded and re-encoded,
// which is where the '?' comes from -- except when it trimmed nothing at all,
// in which case php hands the original string straight back.
$bad = "\xFF\xFEab\xFF";
foreach (["mb_trim", "mb_ltrim", "mb_rtrim"] as $fn) {
    echo $fn, " ", bin2hex($fn($bad)), " ", bin2hex($fn($bad, "\xFF")), " ",
        bin2hex($fn("\xC3\x28", "\xFF")), "\n";
}
// A no-op trim preserves bytes php could not read; a real one substitutes.
var_dump(bin2hex(mb_trim("\xC3\x28")), bin2hex(mb_trim(" \xC3\x28 ")));

// Whole multibyte characters, php's Unicode whitespace default, and the
// nothing-to-strip cases.
$cases = [
    ["  a  ", null], ["\u{3000}\u{2028}y\u{205F}", null], ["\u{00A0}x\u{00A0}", null],
    ["\u{0085}z\u{0085}", null], ["\u{180E}q\u{180E}", null], ["\0\t\n\x0B\f\r x \r\0", null],
    ["   ", null], ["", null], ["abcba", "abc"], ["abcba", ""], ["aaa", "a"],
    ["\u{1F600}a\u{1F600}", "\u{1F600}"], ["\u{200B}m\u{200B}", null],
];
foreach ($cases as [$s, $c]) {
    foreach (["mb_trim", "mb_ltrim", "mb_rtrim"] as $fn) {
        echo bin2hex($c === null ? $fn($s) : $fn($s, $c)), "|";
    }
    echo "\n";
}

// $encoding is screened, not ignored: PHL's scope is the UTF-8 family plus the
// byte encodings, and anything else is php's ValueError.
foreach (["UTF-8", "utf8", "ASCII", "8bit", "nope"] as $enc) {
    try {
        echo $enc, "=", mb_trim("ab", "b", $enc), "\n";
    } catch (ValueError $e) {
        echo $enc, "=", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
foreach ([[[1]], ["a", [1]], ["a", "b", [1]]] as $args) {
    try {
        mb_trim(...$args);
    } catch (TypeError $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
try {
    mb_ltrim();
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
try {
    mb_rtrim("a", "b", "UTF-8", 1);
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
mb_trim fffe6162ff 6162 28
mb_ltrim fffe6162ff 61623f 28
mb_rtrim fffe6162ff 3f3f6162 c328
string(4) "c328"
string(4) "3f28"
61|612020|202061|
79|79e2819f|e38080e280a879|
78|78c2a0|c2a078|
7a|7ac285|c2857a|
71|71e1a08e|e1a08e71|
78|78200d00|00090a0b0c0d2078|
|||
|||
|||
6162636261|6162636261|6162636261|
|||
61|61f09f9880|f09f988061|
e2808b6de2808b|e2808b6de2808b|e2808b6de2808b|
UTF-8=a
utf8=a
ASCII=a
8bit=a
nope=nope=ValueError: mb_trim(): Argument #3 ($encoding) must be a valid encoding, "nope" given
TypeError: mb_trim(): Argument #1 ($string) must be of type string, array given
TypeError: mb_trim(): Argument #2 ($characters) must be of type ?string, array given
TypeError: mb_trim(): Argument #3 ($encoding) must be of type ?string, array given
mb_ltrim() expects at least 1 argument, 0 given
mb_rtrim() expects at most 3 arguments, 4 given
--CLEAN--
<?php
