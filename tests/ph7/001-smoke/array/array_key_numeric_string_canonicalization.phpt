--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A numeric-string array key becomes an integer key only under php's exact rule
--FILE--
<?php
// php canonicalises a string key to an integer only when it is "0" or an
// optionally '-'-signed run of digits with NO leading zero that fits a signed
// 64-bit int. PHL tested the leading zero BEFORE skipping the sign and also
// accepted a leading '+', so "-0"/"+0"/"+1"/"-01" turned into the integer keys
// 0/0/1/-1 — silently COLLIDING with a genuine entry.
$keys = ['0', '-0', '+0', '1', '+1', '01', '-1', '-01', '007', '00',
         ' 1', '1 ', '1.0', '1e2', '0x1', '1_0', '',
         '9223372036854775807', '9223372036854775808', '-9223372036854775808'];
foreach ($keys as $k) {
    $a = [];
    $a[$k] = 1;
    foreach ($a as $kk => $_) {
        echo str_pad(var_export($k, true), 24), gettype($kk), ' ', var_export($kk, true), "\n";
    }
}

// The collision itself: five distinct keys stay five distinct elements.
$a = ['-0' => 'a', '+1' => 'b', '-01' => 'c', '0' => 'd', '-1' => 'e'];
var_export(array_keys($a));
echo "\n";

// ...and every consumer of the key agrees.
var_export(array_flip(['x' => '-0']));
echo "\n";
echo serialize(['-0' => 1]), "\n";
var_export(array_count_values(['-0', '+1']));
echo "\n";
var_export(array_merge(['-0' => 1], ['-0' => 2]));
echo "\n";
?>
--EXPECT--
'0'                     integer 0
'-0'                    string '-0'
'+0'                    string '+0'
'1'                     integer 1
'+1'                    string '+1'
'01'                    string '01'
'-1'                    integer -1
'-01'                   string '-01'
'007'                   string '007'
'00'                    string '00'
' 1'                    string ' 1'
'1 '                    string '1 '
'1.0'                   string '1.0'
'1e2'                   string '1e2'
'0x1'                   string '0x1'
'1_0'                   string '1_0'
''                      string ''
'9223372036854775807'   integer 9223372036854775807
'9223372036854775808'   string '9223372036854775808'
'-9223372036854775808'  integer -9223372036854775807-1
array (
  0 => '-0',
  1 => '+1',
  2 => '-01',
  3 => 0,
  4 => -1,
)
array (
  '-0' => 'x',
)
a:1:{s:2:"-0";i:1;}
array (
  '-0' => 1,
  '+1' => 1,
)
array (
  '-0' => 2,
)
--CLEAN--
<?php
unset($keys, $k, $a);
