--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in exponent notation
--FILE--
<?php
echo 1e1_0 . "\n";
echo 1_0e5 . "\n";
echo 1.5e1_0 . "\n";
echo 2.5_5e+1_0 . "\n";
echo 1_000e-3 . "\n";
echo (int)(1e1_0 === 1e10) . "\n";
echo (int)(1_0e5 === 10e5) . "\n";
echo (int)(1.5e1_0 === 1.5e10) . "\n";
?>
--EXPECT--
10000000000
1000000
15000000000
25500000000
1
1
1
1
--CLEAN--
<?php

