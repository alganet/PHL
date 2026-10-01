--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string(): an expression operand past INT_MAX wraps the way a 64-bit long does
--SKIPIF--
<?php
// php reads each operand with strtol() and narrows it to an int, so the answer
// is the platform's C long. Windows' is 32 bits: strtol() saturates there and
// every row below reads 2147483647 -- the engine answers the 64-bit rows on
// every platform, so neither engine runs this on Windows.
if (PHP_OS_FAMILY === 'Windows') {
    echo 'skip the operand width is the C long, 32 bits on Windows';
}
?>
--FILE--
<?php
foreach (['a = 2147483647|0', 'a = 2147483648|0', 'a = 4294967296|0',
          'a = 99999999999999999999|0'] as $ini) {
    echo $ini, "\n";
    $r = parse_ini_string($ini, false, INI_SCANNER_NORMAL);
    echo '  NORMAL ', $r === false ? 'PARSE-FAILED' : var_export($r['a'], true), "\n";
    $r = parse_ini_string($ini, false, INI_SCANNER_TYPED);
    echo '  TYPED  ', $r === false ? 'PARSE-FAILED' : var_export($r['a'], true), "\n";
}
?>
--EXPECT--
a = 2147483647|0
  NORMAL '2147483647'
  TYPED  2147483647
a = 2147483648|0
  NORMAL '-2147483648'
  TYPED  -2147483648
a = 4294967296|0
  NORMAL '0'
  TYPED  0
a = 99999999999999999999|0
  NORMAL '-1'
  TYPED  -1
