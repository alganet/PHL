--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
soundex skips non-letters and reads high bytes as non-letters
--FILE--
<?php
// A non-letter never separates two consonants sharing a code — php SKIPS it,
// where the old implementation reset the run state.
foreach (["Ss", "S s", "S-S", "b1b", "bab", "Pfister", "Tymczak", "Ashcraft"] as $s) {
  printf("%-10s => %s\n", $s, soundex($s));
}
// Classification is ASCII-only: a high byte is not a letter, and a leading
// accent no longer hides the letters behind it.
foreach (["\xff\xfe", "\xc3\xa9a", "\xc3\x80BC", "\xe9"] as $s) {
  printf("%-10s => %s\n", bin2hex($s), soundex($s));
}
// The whole php string is walked, embedded NUL included.
var_dump(soundex("a\0b"));
// Every letter, folded both ways.
var_dump(soundex("abcdefghijklmnopqrstuvwxyz"));
var_dump(soundex("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
?>
--EXPECT--
Ss         => S000
S s        => S000
S-S        => S000
b1b        => B000
bab        => B100
Pfister    => P236
Tymczak    => T522
Ashcraft   => A226
fffe       => 0000
c3a961     => A000
c3804243   => B200
e9         => 0000
string(4) "A100"
string(4) "A123"
string(4) "A123"
