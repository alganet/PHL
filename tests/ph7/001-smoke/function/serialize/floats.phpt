--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() floats: shortest round-trip, decimal vs exponential
--FILE--
<?php

foreach ([0.0,-0.0,0.1,3.14,1.0,1.5,2.5,0.3,1/3,100.0,123456789.0,0.0001,
          1e16,1e17,1e20,1.23e20,1.5e300,1e100,1e-5,1e-7,9.99e-5] as $f) echo serialize($f),"\n";
?>
--EXPECT--
d:0;
d:-0;
d:0.1;
d:3.14;
d:1;
d:1.5;
d:2.5;
d:0.3;
d:0.3333333333333333;
d:100;
d:123456789;
d:0.0001;
d:10000000000000000;
d:1.0E+17;
d:1.0E+20;
d:1.23E+20;
d:1.5E+300;
d:1.0E+100;
d:1.0E-5;
d:1.0E-7;
d:9.99E-5;
