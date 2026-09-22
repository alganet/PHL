--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode escapes non-ASCII as \uXXXX unless JSON_UNESCAPED_UNICODE
--FILE--
<?php
// php escapes every non-ASCII code point as \uXXXX by default; a code point
// above the BMP goes out as a UTF-16 surrogate pair.
$s = ["é", "日本語", "😀", "\u{FFFF}", "abc", "\u{2028}\u{2029}"];
foreach ($s as $v) {
  printf("%-20s def=%-26s uu=%s\n", bin2hex($v), json_encode($v),
    json_encode($v, JSON_UNESCAPED_UNICODE));
}
// DEL is ASCII, so it goes out raw — php only escapes below 0x20.
echo bin2hex(json_encode("a\u{7f}b")), "\n";
// U+2028/U+2029 stay escaped even under JSON_UNESCAPED_UNICODE unless
// JSON_UNESCAPED_LINE_TERMINATORS says otherwise.
echo bin2hex(json_encode("\u{2028}", JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_LINE_TERMINATORS)), "\n";
echo JSON_UNESCAPED_LINE_TERMINATORS, "\n";
// Keys escape the same way.
echo json_encode(["é" => "ü"]), "\n";
echo json_encode(["é" => "ü"], JSON_UNESCAPED_UNICODE), "\n";
echo json_encode((object)["日" => 1]), "\n";
// ...and it all round-trips through json_decode().
var_dump(json_decode(json_encode(["日本語 😀" => "é"]), true));
?>
--EXPECT--
c3a9                 def="\u00e9"                   uu="é"
e697a5e69cace8aa9e   def="\u65e5\u672c\u8a9e"       uu="日本語"
f09f9880             def="\ud83d\ude00"             uu="😀"
efbfbf               def="\uffff"                   uu="￿"
616263               def="abc"                      uu="abc"
e280a8e280a9         def="\u2028\u2029"             uu="\u2028\u2029"
22617f6222
22e280a822
2048
{"\u00e9":"\u00fc"}
{"é":"ü"}
{"\u65e5":1}
array(1) {
  ["日本語 😀"]=>
  string(2) "é"
}
