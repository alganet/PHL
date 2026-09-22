--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode refuses malformed UTF-8 (JSON_ERROR_UTF8) unless told to ignore or substitute
--FILE--
<?php
// php REFUSES to encode a string that is not UTF-8 — there is no honest JSON
// spelling for a byte that is not text.
$t = ["\xff", "\xe0\xa0", "a\xffb", "\xed\xa0\x80", "\xc0\x80", "\xf5\x80\x80\x80",
      "\xc3\x28", "\xf0\x9f\x98"];
foreach ($t as $s) {
  $r = json_encode($s);
  printf("%-12s def=%-6s err=%d sub=%-22s ign=%s\n", bin2hex($s), var_export($r, true),
    json_last_error(),
    var_export(json_encode($s, JSON_INVALID_UTF8_SUBSTITUTE), true),
    var_export(json_encode($s, JSON_INVALID_UTF8_IGNORE), true));
}
// The failure propagates out of containers and out of KEYS.
var_dump(json_encode(["a" => "\xff"]), json_last_error());
var_dump(json_encode(["\xff" => "a"]), json_last_error());
var_dump(json_encode([["\xff"]]), json_last_error());
var_dump(json_encode((object)["a" => "\xff"]), json_last_error());
var_dump(json_encode(["a" => "\xff"], JSON_INVALID_UTF8_SUBSTITUTE));
// The error state clears on the next good encode.
json_encode("ok"); var_dump(json_last_error());
json_encode("\xff"); var_dump(json_last_error_msg());
// JSON_THROW_ON_ERROR carries the code.
try { json_encode("\xff", JSON_THROW_ON_ERROR); }
catch (Throwable $e) { printf("%s: %s (%d)\n", get_class($e), $e->getMessage(), $e->getCode()); }
// Substitution honours JSON_UNESCAPED_UNICODE.
var_dump(bin2hex(json_encode("a\xffb", JSON_INVALID_UTF8_SUBSTITUTE | JSON_UNESCAPED_UNICODE)));
var_dump(JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE);
?>
--EXPECT--
ff           def=false  err=5 sub='"\\ufffd"'            ign='""'
e0a0         def=false  err=5 sub='"\\ufffd"'            ign='""'
61ff62       def=false  err=5 sub='"a\\ufffdb"'          ign='"ab"'
eda080       def=false  err=5 sub='"\\ufffd"'            ign='""'
c080         def=false  err=5 sub='"\\ufffd\\ufffd"'     ign='""'
f5808080     def=false  err=5 sub='"\\ufffd\\ufffd\\ufffd\\ufffd"' ign='""'
c328         def=false  err=5 sub='"\\ufffd("'           ign='"("'
f09f98       def=false  err=5 sub='"\\ufffd"'            ign='""'
bool(false)
int(5)
bool(false)
int(5)
bool(false)
int(5)
bool(false)
int(5)
string(14) "{"a":"\ufffd"}"
int(0)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"
JsonException: Malformed UTF-8 characters, possibly incorrectly encoded (5)
string(14) "2261efbfbd6222"
int(1048576)
int(2097152)
