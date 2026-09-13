--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode honors JSON_PRETTY_PRINT (4-space indent, nested arrays/objects, empty containers)
--DESCRIPTION--
Regression: JSON_PRETTY_PRINT was a no-op (compact output). It now lays out each
member on its own line indented four spaces per nesting level, a space after each
colon, and keeps empty [] / {} tight — byte-identical to php for both list arrays
and stdClass objects (the shape json_decode(..., false) returns).
--FILE--
<?php
$data = [
    'name'      => 'John',
    'age'       => 5,
    'tags'      => ['a', 'b'],
    'meta'      => ['x' => 1, 'nested' => ['y' => -2]],
    'empty_arr' => [],
    'empty_obj' => new stdClass(),
];
echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES), "\n";
echo "====\n";
$obj = json_decode('{"a":1,"b":{"c":-2,"d":[1,2]}}');
echo json_encode($obj, JSON_PRETTY_PRINT), "\n";
?>
--EXPECT--
{
    "name": "John",
    "age": 5,
    "tags": [
        "a",
        "b"
    ],
    "meta": {
        "x": 1,
        "nested": {
            "y": -2
        }
    },
    "empty_arr": [],
    "empty_obj": {}
}
====
{
    "a": 1,
    "b": {
        "c": -2,
        "d": [
            1,
            2
        ]
    }
}
--CLEAN--
<?php
