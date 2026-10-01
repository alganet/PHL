--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_get_encoding() answers default_charset for all three of php's iconv encodings
--FILE--
<?php
/* iconv_get_encoding() answers php's three iconv encodings:
 * `iconv.input_encoding`, `output_encoding` and `internal_encoding`, each
 * falling back to `default_charset`. The scope policy removes all three directives — every
 * one of them is deprecated, which is also why the SETTER is not here at all
 * (see iconv_set_encoding_refused.phpt) — so `default_charset` is what all
 * three answer, and moving it moves them together, exactly as php does when
 * the iconv directives are left unset. Process-isolated because it writes an
 * engine-wide setting. */
set_error_handler(function ($no, $str) { echo "  W: $str\n"; return true; });
var_dump(iconv_get_encoding());
var_dump(iconv_get_encoding("all") === iconv_get_encoding());
/* Every $type matches case-insensitively. */
foreach (["ALL", "input_encoding", "INPUT_ENCODING", "output_encoding",
          "Internal_Encoding"] as $type) {
    echo str_pad($type, 20), " ", var_export(iconv_get_encoding($type), true), "\n";
}
/* And anything else is a plain false, with no diagnostic of any kind. */
foreach (["bogus", "", "internal", "encoding", "123"] as $type) {
    echo str_pad(var_export($type, true), 20), " ",
         var_export(iconv_get_encoding($type), true), "\n";
}
/* The three move with default_charset, together. */
ini_set('default_charset', 'ISO-8859-1');
var_dump(iconv_get_encoding("all"));
var_dump(iconv_get_encoding("internal_encoding"));
ini_set('default_charset', 'UTF-8');
var_dump(iconv_get_encoding("internal_encoding"));
restore_error_handler();
?>
--EXPECT--
array(3) {
  ["input_encoding"]=>
  string(5) "UTF-8"
  ["output_encoding"]=>
  string(5) "UTF-8"
  ["internal_encoding"]=>
  string(5) "UTF-8"
}
bool(true)
ALL                  array (
  'input_encoding' => 'UTF-8',
  'output_encoding' => 'UTF-8',
  'internal_encoding' => 'UTF-8',
)
input_encoding       'UTF-8'
INPUT_ENCODING       'UTF-8'
output_encoding      'UTF-8'
Internal_Encoding    'UTF-8'
'bogus'              false
''                   false
'internal'           false
'encoding'           false
'123'                false
array(3) {
  ["input_encoding"]=>
  string(10) "ISO-8859-1"
  ["output_encoding"]=>
  string(10) "ISO-8859-1"
  ["internal_encoding"]=>
  string(10) "ISO-8859-1"
}
string(10) "ISO-8859-1"
string(5) "UTF-8"
--CLEAN--
<?php
