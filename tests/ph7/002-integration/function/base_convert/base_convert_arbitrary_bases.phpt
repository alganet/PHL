--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with arbitrary bases (2-36)
--FILE--
<?php
echo base_convert("z", 36, 10) . "\n";
echo base_convert("35", 10, 36) . "\n";
echo base_convert("120", 3, 10) . "\n";
echo base_convert("15", 10, 3) . "\n";
echo base_convert("34", 5, 10) . "\n";
echo base_convert("19", 10, 5) . "\n";
echo base_convert("a", 11, 10) . "\n";
echo base_convert("10", 10, 11) . "\n";
echo base_convert("k", 21, 10) . "\n";
echo base_convert("20", 10, 21) . "\n";
?>
--EXPECT--
35
z
15
120
19
34
10
a
20
k
--CLEAN--
<?php

