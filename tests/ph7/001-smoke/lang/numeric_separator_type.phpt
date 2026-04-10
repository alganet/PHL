--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator type checks
--FILE--
<?php
echo (int)is_int(1_000) . "\n";
echo (int)is_int(0xFF_FF) . "\n";
echo (int)is_int(0b1_0) . "\n";
echo (int)is_int(0_755) . "\n";
echo (int)is_float(1_000.5) . "\n";
echo (int)is_float(1.5e1_0) . "\n";
echo (int)is_numeric(1_000) . "\n";
echo (int)is_numeric(1_000.5) . "\n";
echo (int)is_numeric(0xFF_FF) . "\n";
echo (int)is_numeric(0b1010_1010) . "\n";
?>
--EXPECT--
1
1
1
1
1
1
1
1
1
1
--CLEAN--
<?php

