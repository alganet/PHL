--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_response_code keeps one code, unset until written
--FILE--
<?php
// Nothing prints until the end: a response that has begun refuses every set.
$hrcR = [];
$hrcR['unset_read'] = http_response_code();
$hrcR['first_set'] = http_response_code('500');
$hrcR['read'] = http_response_code();
$hrcR['zero_is_a_read'] = http_response_code(0);
$hrcR['no_range_check'] = http_response_code(-5);
$hrcR['read_again'] = http_response_code();
$hrcR['bool_coerces'] = http_response_code(true);
$hrcR['read_last'] = http_response_code();
try { http_response_code([]); } catch (Throwable $hrcE) { $hrcR['type'] = get_class($hrcE) . ': ' . $hrcE->getMessage(); }
try { http_response_code(1, 2); } catch (Throwable $hrcE) { $hrcR['arity'] = get_class($hrcE) . ': ' . $hrcE->getMessage(); }
var_dump($hrcR);
?>
--EXPECT--
array(10) {
  ["unset_read"]=>
  bool(false)
  ["first_set"]=>
  bool(true)
  ["read"]=>
  int(500)
  ["zero_is_a_read"]=>
  int(500)
  ["no_range_check"]=>
  int(500)
  ["read_again"]=>
  int(-5)
  ["bool_coerces"]=>
  int(-5)
  ["read_last"]=>
  int(1)
  ["type"]=>
  string(94) "TypeError: http_response_code(): Argument #1 ($response_code) must be of type int, array given"
  ["arity"]=>
  string(76) "ArgumentCountError: http_response_code() expects at most 1 argument, 2 given"
}
--CLEAN--
<?php
unset($hrcR, $hrcE);
