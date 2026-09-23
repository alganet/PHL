--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode()/json_validate() $depth counts containers (empty ones included), default 512
--FILE--
<?php
// A container at 1-based nesting L needs $depth > L, even when empty.
var_dump(json_decode("[]", true, 1));
echo json_last_error(), "\n";
var_dump(json_decode("[]", true, 2));
var_dump(json_decode("[[]]", true, 2));
echo json_last_error(), "\n";
var_dump(json_decode("[1]", true, 1));
echo json_last_error(), "\n";
var_dump(json_decode("[1]", true, 2));
// Scalars never consult $depth.
var_dump(json_decode("7", true, 1));
var_dump(json_validate("[]", 1));
echo json_last_error(), "\n";
var_dump(json_validate("[[1]]", 3));
// The default is php's 512, not an engine limit: 40 deep decodes fine,
// 513 deep is JSON_ERROR_DEPTH, and an explicit $depth lifts it.
var_dump(json_decode(str_repeat("[", 40) . str_repeat("]", 40)) !== null);
echo json_last_error(), "\n";
var_dump(json_decode(str_repeat("[", 513) . str_repeat("]", 513), true));
echo json_last_error(), "\n";
var_dump(json_decode(str_repeat("[", 600) . str_repeat("]", 600), true, 700) !== null);
echo json_last_error(), "\n";
?>
--EXPECT--
NULL
1
array(0) {
}
NULL
1
NULL
1
array(1) {
  [0]=>
  int(1)
}
int(7)
bool(false)
1
bool(true)
bool(true)
0
NULL
1
bool(true)
0
--CLEAN--
<?php
