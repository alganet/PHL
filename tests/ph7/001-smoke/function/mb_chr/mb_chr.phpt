--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_chr encodes a code point to UTF-8 and mb_ord decodes the first character
--FILE--
<?php
$o = [];
foreach ([65, 0xE9, 0x20AC, 0x1F600, 0, 0x10FFFF, 0x110000, -1, 0xD800] as $c) {
    $r = mb_chr($c);
    $o[] = $c . "=" . ($r === false ? "F" : bin2hex($r));
}
echo implode(" ", $o), "\n";
$p = [];
foreach (["A", "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "ab", " \xc3\xa9"] as $s) {
    $p[] = mb_ord($s);
}
echo implode(" ", $p), "\n";
$q = [];
foreach (["\xFF", "\xC3\x28", "\xE2\x82"] as $s) {
    $q[] = var_export(mb_ord($s), true);
}
echo implode(" ", $q), "\n";
try { mb_ord(""); } catch (\ValueError $e) { echo $e->getMessage(); }
echo "\n";
--EXPECT--
65=41 233=c3a9 8364=e282ac 128512=f09f9880 0=00 1114111=f48fbfbf 1114112=F -1=F 55296=F
65 233 8364 128512 97 32
false false false
mb_ord(): Argument #1 ($string) must not be empty
--CLEAN--
<?php
