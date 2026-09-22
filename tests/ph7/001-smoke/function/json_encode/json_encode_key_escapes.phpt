--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode escapes object KEYS exactly like string values
--FILE--
<?php
// A key carrying a quote, a backslash or a control character used to be emitted
// raw, which produced JSON no parser accepts.
echo json_encode(["a\"b" => 1]), "\n";
echo json_encode(["a\\b" => 1]), "\n";
echo json_encode(["a\nb\tc" => 1]), "\n";
echo json_encode(["a\x01b" => 1]), "\n";
echo json_encode(["a/b" => 1]), "\n";
echo json_encode(["a/b" => 1], JSON_UNESCAPED_SLASHES), "\n";
echo json_encode(["<>&'\"" => 1], JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT), "\n";
echo json_encode((object)["x\ty" => 1]), "\n";
var_dump(json_decode(json_encode(["a\"b\nc" => "v\"w"]), true));
?>
--EXPECT--
{"a\"b":1}
{"a\\b":1}
{"a\nb\tc":1}
{"a\u0001b":1}
{"a\/b":1}
{"a/b":1}
{"\u003C\u003E\u0026\u0027\u0022":1}
{"x\ty":1}
array(1) {
  ["a"b
c"]=>
  string(3) "v"w"
}
