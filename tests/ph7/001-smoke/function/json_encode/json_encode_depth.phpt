--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode() honors $depth: containers count, scalars are exempt (PHP 8)
--FILE--
<?php
// A container may only open while fewer than $depth containers enclose it.
var_dump(json_encode([1], 0, 1));
var_dump(json_encode([[1]], 0, 1));
echo json_last_error(), "\n";
var_dump(json_encode([[1]], 0, 2));
// Depth 0 (and negative): every container fails, a scalar still encodes.
var_dump(json_encode([1], 0, 0));
echo json_last_error(), "\n";
var_dump(json_encode(1, 0, 0));
var_dump(json_encode("s", 0, -3));
// An object is a container; a backed enum case and a scalar jsonSerialize()
// result are not.
var_dump(json_encode(new stdClass, 0, 0));
echo json_last_error(), "\n";
enum JsonDepthBacked: int { case A = 7; }
var_dump(json_encode(JsonDepthBacked::A, 0, 0));
class JsonDepthScalarSer implements JsonSerializable {
    public function jsonSerialize(): mixed { return 5; }
}
var_dump(json_encode(new JsonDepthScalarSer, 0, 0));
// A jsonSerialize() container result sits at the OBJECT's nesting level.
class JsonDepthArraySer implements JsonSerializable {
    public function jsonSerialize(): mixed { return [1]; }
}
var_dump(json_encode(new JsonDepthArraySer, 0, 1));
var_dump(json_encode(new JsonDepthArraySer, 0, 0));
echo json_last_error(), "\n";
// The default is php's 512: 511 nested containers pass, 513 do not.
function json_depth_build(int $n): array {
    $a = [];
    $r = &$a;
    for ($i = 1; $i < $n; $i++) { $r[0] = []; $r = &$r[0]; }
    return $a;
}
var_dump(json_encode(json_depth_build(511)) !== false);
var_dump(json_encode(json_depth_build(513)));
echo json_last_error(), "\n";
var_dump(json_encode(json_depth_build(513), 0, 600) !== false);
echo json_last_error(), "\n";
?>
--EXPECT--
string(3) "[1]"
bool(false)
1
string(5) "[[1]]"
bool(false)
1
string(1) "1"
string(3) ""s""
bool(false)
1
string(1) "7"
string(1) "5"
string(3) "[1]"
bool(false)
1
bool(true)
bool(false)
1
bool(true)
0
--CLEAN--
<?php
