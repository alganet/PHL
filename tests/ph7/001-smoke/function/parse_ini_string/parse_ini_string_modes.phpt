--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_string() scanner modes: NORMAL interprets, RAW keeps, both keep every entry
--FILE--
<?php
define('INI_MODES_CONST', 'someval');
putenv('INI_MODES_ENV=fromenv');
$ini = "b1 = true\nb2 = off\nn1 = null\nw1 = none\n"
     . "c1 = INI_MODES_CONST\nc2 = INI_MODES_CONST and more\nc3 = truely\nc4 = \"INI_MODES_CONST\"\n"
     . "v1 = \${INI_MODES_ENV}\nv2 = pre\${INI_MODES_ENV}post\nv3 = \${INI_MODES_UNDEF_XYZ}\n"
     . "k1 = 10K\nq1 = \"true\"\n"
     . "e1 =\nafter = kept\n"
     . "cm = a ; trailing comment\n";
echo "NORMAL:\n";
var_dump(parse_ini_string($ini, false, INI_SCANNER_NORMAL));
echo "RAW:\n";
var_dump(parse_ini_string($ini, false, INI_SCANNER_RAW));
// The default mode IS NORMAL.
var_dump(parse_ini_string("x = yes")["x"]);
// An unknown mode: php's warning (no function qualifier), FALSE, nothing parsed.
set_error_handler(function ($no, $str) { echo "warn: ", $str, "\n"; return true; });
var_dump(parse_ini_string("x=1", false, 7));
restore_error_handler();
?>
--EXPECT--
NORMAL:
array(16) {
  ["b1"]=>
  string(1) "1"
  ["b2"]=>
  string(0) ""
  ["n1"]=>
  string(0) ""
  ["w1"]=>
  string(0) ""
  ["c1"]=>
  string(7) "someval"
  ["c2"]=>
  string(16) "someval and more"
  ["c3"]=>
  string(6) "truely"
  ["c4"]=>
  string(15) "INI_MODES_CONST"
  ["v1"]=>
  string(7) "fromenv"
  ["v2"]=>
  string(14) "prefromenvpost"
  ["v3"]=>
  string(0) ""
  ["k1"]=>
  string(3) "10K"
  ["q1"]=>
  string(4) "true"
  ["e1"]=>
  string(0) ""
  ["after"]=>
  string(4) "kept"
  ["cm"]=>
  string(1) "a"
}
RAW:
array(16) {
  ["b1"]=>
  string(4) "true"
  ["b2"]=>
  string(3) "off"
  ["n1"]=>
  string(4) "null"
  ["w1"]=>
  string(4) "none"
  ["c1"]=>
  string(15) "INI_MODES_CONST"
  ["c2"]=>
  string(24) "INI_MODES_CONST and more"
  ["c3"]=>
  string(6) "truely"
  ["c4"]=>
  string(15) "INI_MODES_CONST"
  ["v1"]=>
  string(16) "${INI_MODES_ENV}"
  ["v2"]=>
  string(23) "pre${INI_MODES_ENV}post"
  ["v3"]=>
  string(22) "${INI_MODES_UNDEF_XYZ}"
  ["k1"]=>
  string(3) "10K"
  ["q1"]=>
  string(4) "true"
  ["e1"]=>
  string(0) ""
  ["after"]=>
  string(4) "kept"
  ["cm"]=>
  string(1) "a"
}
string(1) "1"
warn: Invalid scanner mode
bool(false)
--CLEAN--
<?php
unset($ini);
