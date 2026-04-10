--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in float literals
--FILE--
<?php
echo 1_000.50 . "\n";
echo 3.141_59 . "\n";
echo 1_234.567_89 . "\n";
echo 0.123_456 . "\n";
echo 1_000.0 . "\n";
echo (int)(1_000.50 === 1000.50) . "\n";
echo (int)(3.141_59 === 3.14159) . "\n";
echo (int)(1_234.567_89 === 1234.56789) . "\n";
?>
--EXPECT--
1000.5
3.14159
1234.56789
0.123456
1000
1
1
1
--CLEAN--
<?php

