--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode resolves \uXXXX, \b and rejects undefined escapes
--FILE--
<?php
// Every escape JSON defines, the surrogate-pair form for a code point above the
// BMP, and the ways an escape can be malformed.
$t = [
  '"a\"b"', '"a\\\\b"', '"a\/b"', '"\b\f\n\r\t"',
  '"\u0041"', '"a\u0041b"', '"\u00e9"', '"\uABCD"', '"\u2028"',
  '"\ud83d\ude00"', '"\udbff\udfff"',
  '"\ud83d"', '"\udc00"', '"\ud83d\ud83d"',
  '"\u12"', '"\uZZZZ"', '"\q"',
  // A string ENDING in an escaped backslash: the closing quote is real, not
  // escaped, and used to be read as part of the escape.
  '"\\\\"', '"a\\\\"',
  '"abc',
];
foreach ($t as $j) {
  $r = json_decode($j);
  printf("%-18s => %-12s err=%d %s\n", $j, $r === null ? 'NULL' : bin2hex($r),
    json_last_error(), json_last_error_msg());
}
// Escapes reach object keys too...
var_dump(json_decode('{"\u00e9":"\u00fc"}', true));
// ...and a decoded escape is the same string the php literal spells.
var_dump(json_decode('"\ud83d\ude00"') === "\u{1F600}");
?>
--EXPECT--
"a\"b"             => 612262       err=0 No error
"a\\b"             => 615c62       err=0 No error
"a\/b"             => 612f62       err=0 No error
"\b\f\n\r\t"       => 080c0a0d09   err=0 No error
"\u0041"           => 41           err=0 No error
"a\u0041b"         => 614162       err=0 No error
"\u00e9"           => c3a9         err=0 No error
"\uABCD"           => eaaf8d       err=0 No error
"\u2028"           => e280a8       err=0 No error
"\ud83d\ude00"     => f09f9880     err=0 No error
"\udbff\udfff"     => f48fbfbf     err=0 No error
"\ud83d"           => NULL         err=10 Single unpaired UTF-16 surrogate in unicode escape
"\udc00"           => NULL         err=10 Single unpaired UTF-16 surrogate in unicode escape
"\ud83d\ud83d"     => NULL         err=10 Single unpaired UTF-16 surrogate in unicode escape
"\u12"             => NULL         err=4 Syntax error
"\uZZZZ"           => NULL         err=4 Syntax error
"\q"               => NULL         err=4 Syntax error
"\\"               => 5c           err=0 No error
"a\\"              => 615c         err=0 No error
"abc               => NULL         err=3 Control character error, possibly incorrectly encoded
array(1) {
  ["é"]=>
  string(2) "ü"
}
bool(true)
