--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_* substitute '?' for ill-formed UTF-8 instead of reading it as latin-1
--FILE--
<?php
// php substitutes '?' for every ill-formed run rather than reinterpreting the
// bytes as latin-1, and counts that run as ONE character.
$t = ["\xff\xfe", "\xff", "a\xffb", "\xc3", "\xc3\x28", "\xe0\xa0",
      "\xed\xa0\x80", "\xf5\x80\x80\x80", "\xc0\x80", "\x80\x81",
      "\xf0\x9f\x98", "ab\xe0\xa0cd"];
foreach ($t as $s) {
  printf("%-14s len=%d lower=%-14s upper=%-14s title=%-14s sub=%s\n", bin2hex($s),
    mb_strlen($s), bin2hex(mb_strtolower($s)), bin2hex(mb_strtoupper($s)),
    bin2hex(mb_convert_case($s, MB_CASE_TITLE)), bin2hex(mb_substr($s, 0, 20)));
}
// The substitution is not a word boundary, and not a word character either.
var_dump(bin2hex(mb_convert_case("\xffab", MB_CASE_TITLE)));
var_dump(bin2hex(mb_convert_case("a\xffb", MB_CASE_TITLE)));
// mb_ord and mb_check_encoding reject what the decoder rejects: over-long,
// surrogate and past-U+10FFFF forms included.
foreach (["\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc3\xa9"] as $s) {
  printf("%-10s ord=%-6s check=%s\n", bin2hex($s), var_export(mb_ord($s), true),
    var_export(mb_check_encoding($s, 'UTF-8'), true));
}
// A well-formed string is untouched.
var_dump(mb_strtoupper("héllo wörld"), mb_substr("日本語テキスト", 2, 3));
?>
--EXPECT--
fffe           len=2 lower=3f3f           upper=3f3f           title=3f3f           sub=3f3f
ff             len=1 lower=3f             upper=3f             title=3f             sub=3f
61ff62         len=3 lower=613f62         upper=413f42         title=413f62         sub=613f62
c3             len=1 lower=3f             upper=3f             title=3f             sub=3f
c328           len=2 lower=3f28           upper=3f28           title=3f28           sub=3f28
e0a0           len=1 lower=3f             upper=3f             title=3f             sub=3f
eda080         len=3 lower=3f3f3f         upper=3f3f3f         title=3f3f3f         sub=3f3f3f
f5808080       len=4 lower=3f3f3f3f       upper=3f3f3f3f       title=3f3f3f3f       sub=3f3f3f3f
c080           len=2 lower=3f3f           upper=3f3f           title=3f3f           sub=3f3f
8081           len=2 lower=3f3f           upper=3f3f           title=3f3f           sub=3f3f
f09f98         len=1 lower=3f             upper=3f             title=3f             sub=3f
6162e0a06364   len=5 lower=61623f6364     upper=41423f4344     title=41623f6364     sub=61623f6364
string(6) "3f4162"
string(6) "413f62"
c080       ord=false  check=false
eda080     ord=false  check=false
f4908080   ord=false  check=false
c3a9       ord=233    check=true
string(13) "HÉLLO WÖRLD"
string(9) "語テキ"
