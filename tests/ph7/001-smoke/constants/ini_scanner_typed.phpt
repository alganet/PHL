--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: INI_SCANNER_TYPED constant and typed value mapping
--FILE--
<?php
echo "INI_SCANNER_TYPED=", INI_SCANNER_TYPED, "\n";
$ini = "b1 = true\nb2 = ON\nb3 = yes\nb4 = False\nb5 = off\nb6 = no\nb7 = none\n"
     . "n1 = null\n"
     . "i1 = 42\ni2 = -7\ni3 = 017\ni4 = 9223372036854775807\ni5 = 9223372036854775808\n"
     . "f1 = 1.5\nf2 = .5\nf3 = 5.\n"
     . "s1 = 0x1A\ns2 = 1e3\ns3 = 42abc\ns4 = +5\ns5 = -1.5\ns6 = \"42\"\ns7 = \"true\"\ne1 =\n";
var_dump(parse_ini_string($ini, false, INI_SCANNER_TYPED));
// Sections type their values too.
var_dump(parse_ini_string("[s]\nb = true\nn = 5", true, INI_SCANNER_TYPED));
?>
--EXPECT--
INI_SCANNER_TYPED=2
array(24) {
  ["b1"]=>
  bool(true)
  ["b2"]=>
  bool(true)
  ["b3"]=>
  bool(true)
  ["b4"]=>
  bool(false)
  ["b5"]=>
  bool(false)
  ["b6"]=>
  bool(false)
  ["b7"]=>
  bool(false)
  ["n1"]=>
  NULL
  ["i1"]=>
  int(42)
  ["i2"]=>
  int(-7)
  ["i3"]=>
  int(17)
  ["i4"]=>
  int(9223372036854775807)
  ["i5"]=>
  string(19) "9223372036854775808"
  ["f1"]=>
  float(1.5)
  ["f2"]=>
  float(0.5)
  ["f3"]=>
  float(5)
  ["s1"]=>
  string(4) "0x1A"
  ["s2"]=>
  string(3) "1e3"
  ["s3"]=>
  string(5) "42abc"
  ["s4"]=>
  string(2) "+5"
  ["s5"]=>
  string(4) "-1.5"
  ["s6"]=>
  string(2) "42"
  ["s7"]=>
  string(4) "true"
  ["e1"]=>
  string(0) ""
}
array(1) {
  ["s"]=>
  array(2) {
    ["b"]=>
    bool(true)
    ["n"]=>
    int(5)
  }
}
--CLEAN--
<?php
unset($ini);
