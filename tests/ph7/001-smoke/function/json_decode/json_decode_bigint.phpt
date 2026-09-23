--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
JSON_BIGINT_AS_STRING keeps an over-int64 integer literal as its exact source text
--FILE--
<?php
// Beyond int64 either way: exact source text as a string.
var_dump(json_decode('[10000000000000000000]', true, 512, JSON_BIGINT_AS_STRING));
var_dump(json_decode('[-9223372036854775809]', true, 512, JSON_BIGINT_AS_STRING));
// Fits: stays an int (INT_MAX and INT_MIN included).
var_dump(json_decode('[9223372036854775807]', true, 512, JSON_BIGINT_AS_STRING));
var_dump(json_decode('[-9223372036854775808]', true, 512, JSON_BIGINT_AS_STRING));
// A float SHAPE is a float regardless of the flag.
var_dump(json_decode('[1e2]', true, 512, JSON_BIGINT_AS_STRING));
var_dump(json_decode('[1.5]', true, 512, JSON_BIGINT_AS_STRING));
// Without the flag a big integer keeps landing on a float.
var_dump(json_decode('[10000000000000000000]', true));
?>
--EXPECT--
array(1) {
  [0]=>
  string(20) "10000000000000000000"
}
array(1) {
  [0]=>
  string(20) "-9223372036854775809"
}
array(1) {
  [0]=>
  int(9223372036854775807)
}
array(1) {
  [0]=>
  int(-9223372036854775808)
}
array(1) {
  [0]=>
  float(100)
}
array(1) {
  [0]=>
  float(1.5)
}
array(1) {
  [0]=>
  float(1.0E+19)
}
--CLEAN--
<?php
