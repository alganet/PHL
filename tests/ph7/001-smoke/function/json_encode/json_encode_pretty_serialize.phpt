--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
JSON_PRETTY_PRINT indents a jsonSerialize() result at the object's own level
--FILE--
<?php
class JsonPrettySer implements JsonSerializable {
    public function jsonSerialize(): mixed { return [1, 2]; }
}
echo json_encode(["a" => new JsonPrettySer], JSON_PRETTY_PRINT), "\n";
echo json_encode(new JsonPrettySer, JSON_PRETTY_PRINT), "\n";
?>
--EXPECT--
{
    "a": [
        1,
        2
    ]
}
[
    1,
    2
]
--CLEAN--
<?php
