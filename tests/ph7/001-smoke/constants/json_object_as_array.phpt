--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_OBJECT_AS_ARRAY constant and its $associative interplay
--FILE--
<?php
echo "JSON_OBJECT_AS_ARRAY=" . JSON_OBJECT_AS_ARRAY . "\n";
// The flag answers only when $associative is NULL; an explicit false wins.
echo gettype(json_decode('{"a":1}', null, 512, JSON_OBJECT_AS_ARRAY)), "\n";
echo gettype(json_decode('{"a":1}', false, 512, JSON_OBJECT_AS_ARRAY)), "\n";
echo gettype(json_decode('{"a":1}', true)), "\n";
echo gettype(json_decode('{"a":1}')), "\n";
// Nested objects convert too.
var_dump(json_decode('{"a":{"b":2}}', null, 512, JSON_OBJECT_AS_ARRAY));
?>
--EXPECT--
JSON_OBJECT_AS_ARRAY=1
array
object
array
object
array(1) {
  ["a"]=>
  array(1) {
    ["b"]=>
    int(2)
  }
}
--CLEAN--
<?php
