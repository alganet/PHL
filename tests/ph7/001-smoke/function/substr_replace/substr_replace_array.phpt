--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_replace array form: per-element positional replace/offset/length, keys preserved
--FILE--
<?php
// scalar replace applied to each element
echo json_encode(substr_replace(["ab", "cd"], "X", 1)), "\n";
// positional arrays consumed element by element
echo json_encode(substr_replace(["ab", "cd"], ["X", "Y"], [0, 1], [1, 1])), "\n";
// exhausted arrays fall back to ""/0/element-length
echo json_encode(substr_replace(["ab", "cd", "ef"], ["X"], [0, 1], 1)), "\n";
echo json_encode(substr_replace(["ab"], "X", [], [])), "\n";
// string keys preserved
echo json_encode(substr_replace(["k1" => "ab", "k2" => "cd"], "X", 0, 1)), "\n";
// non-string elements are stringified
echo json_encode(substr_replace([7, "cd"], "X", 1)), "\n";
// empty subject array
echo json_encode(substr_replace([], "X", 0)), "\n";
?>
--EXPECT--
["aX","cX"]
["Xb","cY"]
["Xb","c","f"]
["X"]
{"k1":"Xb","k2":"Xd"}
["7X","cX"]
[]
--CLEAN--
<?php
