--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode() refuses a resource: JSON_ERROR_UNSUPPORTED_TYPE, not "null"
--FILE--
<?php
$f = fopen("php://memory", "r");
var_dump(json_encode($f));
echo json_last_error(), " ", json_last_error_msg(), "\n";
// Nested: the whole encode fails, not just the one member.
var_dump(json_encode(["ok" => 1, "bad" => $f]));
echo json_last_error(), "\n";
// JSON_THROW_ON_ERROR turns it into a JsonException carrying the same text.
try {
    json_encode($f, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo get_class($e), ": ", $e->getMessage(), " (", $e->getCode(), ")\n";
}
fclose($f);
?>
--EXPECT--
bool(false)
8 Type is not supported
bool(false)
8
JsonException: Type is not supported (8)
--CLEAN--
<?php
