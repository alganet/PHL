--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_SANITIZE_FULL_SPECIAL_CHARS: UTF-8 named entities, verbatim passthrough, invalid->empty, NO_ENCODE_QUOTES
--FILE--
<?php
// bin2hex keeps the comparison byte-exact and file-encoding independent.
if(!function_exists("h")){function h($v){ echo bin2hex($v),"\n"; }}
// valid UTF-8 codepoints with an HTML 4.01 named entity -> that entity
h(filter_var("\xC3\xA9", FILTER_SANITIZE_FULL_SPECIAL_CHARS));          // é   -> &eacute;
h(filter_var("\xE2\x82\xAC", FILTER_SANITIZE_FULL_SPECIAL_CHARS));      // €   -> &euro;
h(filter_var("\xC2\xA9", FILTER_SANITIZE_FULL_SPECIAL_CHARS));          // ©   -> &copy;
h(filter_var("\xC2\xA0", FILTER_SANITIZE_FULL_SPECIAL_CHARS));          // nbsp -> &nbsp;
// specials + named + verbatim (CJK) in one string
h(filter_var("<a>&\"'\xC3\xA9\xE4\xB8\xAD", FILTER_SANITIZE_FULL_SPECIAL_CHARS));
// valid codepoints WITHOUT a named entity pass through verbatim (emoji + CJK)
h(filter_var("\xF0\x9F\x98\x80\xE4\xB8\xAD", FILTER_SANITIZE_FULL_SPECIAL_CHARS));
// control bytes <32 pass through verbatim (not encoded by the FULL filter)
h(filter_var("x\x09y\x00z", FILTER_SANITIZE_FULL_SPECIAL_CHARS));
// ANY invalid UTF-8 makes the whole result empty
h(filter_var("a\xE9b", FILTER_SANITIZE_FULL_SPECIAL_CHARS));            // lone lead byte
h(filter_var("ok\xFF", FILTER_SANITIZE_FULL_SPECIAL_CHARS));            // 0xFF
h(filter_var("\xED\xA0\x80", FILTER_SANITIZE_FULL_SPECIAL_CHARS));      // UTF-16 surrogate
// NO_ENCODE_QUOTES: " and ' pass through; < > & still encoded
h(filter_var("x\"y'z<&", FILTER_SANITIZE_FULL_SPECIAL_CHARS, ["flags"=>FILTER_FLAG_NO_ENCODE_QUOTES]));
?>
--EXPECT--
266561637574653b
266575726f3b
26636f70793b
266e6273703b
266c743b612667743b26616d703b2671756f743b26233033393b266561637574653be4b8ad
f09f9880e4b8ad
780979007a



782279277a266c743b26616d703b
