--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert basic conversions between standard bases
--FILE--
<?php
echo base_convert("a37334", 16, 2) . "\n";
echo base_convert("ff", 16, 10) . "\n";
echo base_convert("255", 10, 16) . "\n";
echo base_convert("1010", 2, 10) . "\n";
echo base_convert("77", 8, 10) . "\n";
echo base_convert("10", 10, 2) . "\n";
echo base_convert("10", 10, 8) . "\n";
echo base_convert("10", 10, 16) . "\n";
?>
--EXPECT--
101000110111001100110100
255
ff
10
63
1010
12
a
--CLEAN--
<?php

