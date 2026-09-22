--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode rejects malformed UTF-8 (JSON_ERROR_UTF8) unless told to ignore or substitute
--FILE--
<?php
// php rejects a JSON document that is not valid UTF-8 with JSON_ERROR_UTF8,
// wherever the offending byte sits.
$t = ["\"\xff\"", "\"a\xffb\"", "[\"\xff\"]", "{\"\xff\":1}", "\xff",
      "[1,2]\xff", "\xff[1,2]", "[1,\xff2]", "\"\xc3\xa9\"", "[1,2] "];
foreach ($t as $j) {
  $r = json_decode($j, true);
  printf("%-14s => %-18s err=%d\n", bin2hex($j),
    $r === null ? 'NULL' : json_encode($r), json_last_error());
}
// A valid non-ASCII character in the wrong place is still a SYNTAX error.
json_decode("é"); var_dump(json_last_error());
// The two opt-outs apply INSIDE strings only.
var_dump(bin2hex((string)json_decode("\"a\xffb\"", false, 512, JSON_INVALID_UTF8_SUBSTITUTE)));
var_dump(bin2hex((string)json_decode("\"a\xffb\"", false, 512, JSON_INVALID_UTF8_IGNORE)));
var_dump(json_decode("{\"a\xffb\":1}", true, 512, JSON_INVALID_UTF8_SUBSTITUTE));
var_dump(json_decode("\xff", false, 512, JSON_INVALID_UTF8_SUBSTITUTE), json_last_error());
// JSON_THROW_ON_ERROR carries the message and the code.
try { json_decode("\"\xff\"", false, 512, JSON_THROW_ON_ERROR); }
catch (Throwable $e) { printf("%s: %s (%d)\n", get_class($e), $e->getMessage(), $e->getCode()); }
// json_validate() rides the same rail: JSON_INVALID_UTF8_IGNORE makes a payload
// with undecodable bytes valid.
var_dump(json_validate("\"a\xffb\""), json_last_error());
var_dump(json_validate("\"a\xffb\"", 512, JSON_INVALID_UTF8_IGNORE), json_last_error());
// php's decoder SUBSTITUTES when both flags are set (its encoder drops instead).
var_dump(bin2hex((string)json_decode("\"a\xffb\"", false, 512,
  JSON_INVALID_UTF8_IGNORE | JSON_INVALID_UTF8_SUBSTITUTE)));
?>
--EXPECT--
22ff22         => NULL               err=5
2261ff6222     => NULL               err=5
5b22ff225d     => NULL               err=5
7b22ff223a317d => NULL               err=5
ff             => NULL               err=5
5b312c325dff   => NULL               err=5
ff5b312c325d   => NULL               err=5
5b312cff325d   => NULL               err=5
22c3a922       => "\u00e9"           err=0
5b312c325d20   => [1,2]              err=0
int(4)
string(10) "61efbfbd62"
string(4) "6162"
array(1) {
  ["a�b"]=>
  int(1)
}
NULL
int(5)
JsonException: Malformed UTF-8 characters, possibly incorrectly encoded (5)
bool(false)
int(5)
bool(true)
int(0)
string(10) "61efbfbd62"
