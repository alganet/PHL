--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_response_code refuses once the body has begun
--FILE--
<?php
$hrcLog = [];
set_error_handler(function ($n, $s) use (&$hrcLog) { $hrcLog[] = "E$n: $s"; return true; });
$hrcOut = [];
echo "OUT\n";
$hrcOut['set'] = http_response_code(500);
$hrcOut['read'] = http_response_code();
restore_error_handler();
var_dump($hrcOut);
foreach ($hrcLog as $hrcLine) { echo str_replace(__FILE__, 'FILE', $hrcLine), "\n"; }
?>
--EXPECT--
OUT
array(2) {
  ["set"]=>
  bool(false)
  ["read"]=>
  bool(false)
}
E2: http_response_code(): Cannot set response code - headers already sent (output started at FILE:5)
--CLEAN--
<?php
unset($hrcLog, $hrcOut, $hrcLine);
