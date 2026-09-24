--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
intval()/(int) read a float-shaped numeric string through its VALUE, not its digits
--FILE--
<?php
/* A numeric string that carries a '.' or an exponent is a DOUBLE first and an
 * int second -- reading its digit run instead stops at the '.' or the 'e' and
 * answers the mantissa's integer part, so "1e3" came out as 1 and "1.5e2" as 1.
 * Out of range the string conversion SATURATES (php's zend_dval_to_lval_cap) and
 * answers 0 for a value that is not finite, which is not what the cast of a real
 * float does in either engine -- see lang/float_to_int_out_of_range. */
foreach ([
    '1e3', '1E3', '1e0', '1E+2', '1e-0', '1e-3', '0e0',
    '1.5e2', '-2e2', '5.e2', '5.0e2', '.5e1', '3.0e1',
    '1e15', '1e18', '9e18', '1e19', '1e30', '-1e30',
    '1e308', '1e309', '1e400', '-1e400',
    '1.9', '-1.9', '0.5', '-0.5', '.5', '-.5', '1.', '0.0', '-0.0', '000.5',
    '0.9999999999999999999', '1.9999999999999999999', '-0.99999999999999999999',
    '9223372036854775296.0', '18446744073709551616.0',
    ' 1e3 ', '1e3abc', '1.5abc', '1e', '1e+',
    '99999999999999999999', '123', 'abc', '',
] as $s) {
    printf("%-26s %-21s %s\n", "'" . $s . "'", intval($s), (int)$s);
}
/* The same rule reaches every other spelling of the conversion. */
$v = '2.5e3';
settype($v, 'integer');
var_dump($v);
var_dump((int)'7.5e1' === 75, intval('7.5e1', 10) === 75);
/* A float-shaped key is NOT canonicalised into an int one, in either engine. */
var_dump(array_keys(['1e3' => 'a', '1.5' => 'b']));
?>
--EXPECT--
'1e3'                      1000                  1000
'1E3'                      1000                  1000
'1e0'                      1                     1
'1E+2'                     100                   100
'1e-0'                     1                     1
'1e-3'                     0                     0
'0e0'                      0                     0
'1.5e2'                    150                   150
'-2e2'                     -200                  -200
'5.e2'                     500                   500
'5.0e2'                    500                   500
'.5e1'                     5                     5
'3.0e1'                    30                    30
'1e15'                     1000000000000000      1000000000000000
'1e18'                     1000000000000000000   1000000000000000000
'9e18'                     9000000000000000000   9000000000000000000
'1e19'                     9223372036854775807   9223372036854775807
'1e30'                     9223372036854775807   9223372036854775807
'-1e30'                    -9223372036854775808  -9223372036854775808
'1e308'                    9223372036854775807   9223372036854775807
'1e309'                    0                     0
'1e400'                    0                     0
'-1e400'                   0                     0
'1.9'                      1                     1
'-1.9'                     -1                    -1
'0.5'                      0                     0
'-0.5'                     0                     0
'.5'                       0                     0
'-.5'                      0                     0
'1.'                       1                     1
'0.0'                      0                     0
'-0.0'                     0                     0
'000.5'                    0                     0
'0.9999999999999999999'    1                     1
'1.9999999999999999999'    2                     2
'-0.99999999999999999999'  -1                    -1
'9223372036854775296.0'    9223372036854775807   9223372036854775807
'18446744073709551616.0'   9223372036854775807   9223372036854775807
' 1e3 '                    1000                  1000
'1e3abc'                   1000                  1000
'1.5abc'                   1                     1
'1e'                       1                     1
'1e+'                      1                     1
'99999999999999999999'     9223372036854775807   9223372036854775807
'123'                      123                   123
'abc'                      0                     0
''                         0                     0
int(2500)
bool(true)
bool(true)
array(2) {
  [0]=>
  string(3) "1e3"
  [1]=>
  string(3) "1.5"
}
