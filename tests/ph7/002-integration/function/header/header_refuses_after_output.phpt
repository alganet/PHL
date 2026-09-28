--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the header family refuses after output and names where it began
--FILE--
<?php
$hdrLog = [];
set_error_handler(function ($n, $s) use (&$hdrLog) { $hdrLog[] = "E$n: $s"; return true; });
$hdrRet = [];
$hdrRet['header_ok'] = header('X-A: 1');
$hdrRet['setcookie_ok'] = setcookie('a', 'b');
echo "OUT\n";
$hdrRet['header'] = header('X-B: 2');
$hdrRet['header_remove'] = header_remove('X-A');
$hdrRet['setcookie'] = setcookie('c', 'd');
$hdrRet['setrawcookie'] = setrawcookie('e', 'f');
restore_error_handler();
var_dump($hdrRet);
foreach ($hdrLog as $hdrLine) { echo str_replace(__FILE__, 'FILE', $hdrLine), "\n"; }
?>
--EXPECT--
OUT
array(6) {
  ["header_ok"]=>
  NULL
  ["setcookie_ok"]=>
  bool(true)
  ["header"]=>
  NULL
  ["header_remove"]=>
  NULL
  ["setcookie"]=>
  bool(false)
  ["setrawcookie"]=>
  bool(false)
}
E2: Cannot modify header information - headers already sent by (output started at FILE:7)
E2: Cannot modify header information - headers already sent by (output started at FILE:7)
E2: Cannot modify header information - headers already sent by (output started at FILE:7)
E2: Cannot modify header information - headers already sent by (output started at FILE:7)
--CLEAN--
<?php
unset($hdrLog, $hdrRet, $hdrLine);
