--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator comparison operators
--FILE--
<?php
echo (int)(1_000 === 1000) . "\n";
echo (int)(1_000 == 1000) . "\n";
echo (int)(1_000 !== 999) . "\n";
echo (int)(0xFF_FF === 0xFFFF) . "\n";
echo (int)(0b1_0 === 2) . "\n";
echo (int)(0_755 === 493) . "\n";
echo (int)(1_000 < 2_000) . "\n";
echo (int)(2_000 > 1_000) . "\n";
echo (int)(1_000 <= 1_000) . "\n";
echo (int)(1_000.5 == 1000.5) . "\n";
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

