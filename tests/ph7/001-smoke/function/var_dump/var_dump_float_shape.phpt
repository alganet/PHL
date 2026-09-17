--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump renders floats at serialize_precision=-1: shortest decimal, php's
gcvt(ndigit=17) fixed-vs-exponential rule (whole floats stay fixed, not 1.5E+3)

--FILE--
<?php
var_dump(1500.0);                 // whole float stays fixed, was 1.5E+3
var_dump(1e2);                    // 100, was 1.0E+2
var_dump(100000000000000.0);      // 1e14 fixed (decpt 15 <= 17)
var_dump(1234567.0);
var_dump(1.0);
var_dump(-1500.0);
var_dump(3.14);
var_dump(0.1);
var_dump(2.5e-3);                 // fixed 0.0025 (decpt -2, not < -3)
var_dump(0.025);
var_dump(1e20);                   // exponential (decpt 21 > 17)
var_dump(1e-7);                   // exponential (decpt -6 < -3)
var_dump(-0.0);                   // php prints float(-0)
var_dump(0.0);
var_dump(1/3);
var_dump([1500.0, 100.0, 0.0025, 1e20]);
?>
--EXPECT--
float(1500)
float(100)
float(100000000000000)
float(1234567)
float(1)
float(-1500)
float(3.14)
float(0.1)
float(0.0025)
float(0.025)
float(1.0E+20)
float(1.0E-7)
float(-0)
float(0)
float(0.3333333333333333)
array(4) {
  [0]=>
  float(1500)
  [1]=>
  float(100)
  [2]=>
  float(0.0025)
  [3]=>
  float(1.0E+20)
}
