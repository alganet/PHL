--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode() detects recursive values: JSON_ERROR_RECURSION, not a truncated answer
--FILE--
<?php
// An array reached through its own reference cycle.
$a = ["x" => 1];
$a["self"] = &$a;
var_dump(json_encode($a));
echo json_last_error(), " ", json_last_error_msg(), "\n";
unset($a["self"]);
// An object holding itself.
$o = new stdClass;
$o->self = $o;
var_dump(json_encode($o));
echo json_last_error(), "\n";
// jsonSerialize() returning $this is php's ONE self-reference exception:
// the property view encodes, no error.
class JsonRecSelfReturn implements JsonSerializable {
    public $tag = "self";
    public function jsonSerialize(): mixed { return $this; }
}
var_dump(json_encode(new JsonRecSelfReturn));
echo json_last_error(), "\n";
// ...but $this INSIDE the result is recursion: the object is still being encoded.
class JsonRecSelfNested implements JsonSerializable {
    public function jsonSerialize(): mixed { return [$this]; }
}
var_dump(json_encode(new JsonRecSelfNested));
echo json_last_error(), "\n";
// Sharing is not recursion: the same value twice as SIBLINGS stays legal.
$leaf = [1];
var_dump(json_encode([$leaf, $leaf, [["x" => $leaf]]]));
echo json_last_error(), "\n";
?>
--EXPECT--
bool(false)
6 Recursion detected
bool(false)
6
string(14) "{"tag":"self"}"
0
bool(false)
6
string(21) "[[1],[1],[{"x":[1]}]]"
0
--CLEAN--
<?php
