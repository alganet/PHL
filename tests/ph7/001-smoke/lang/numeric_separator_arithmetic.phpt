--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator arithmetic operations
--FILE--
<?php
echo 1_000 + 2_000 . "\n";
echo 5_000 - 1_000 . "\n";
echo 1_000 * 3 . "\n";
echo 10_000 / 2_5 . "\n";
echo 1_0 % 3 . "\n";
echo 0xFF_FF + 1 . "\n";
echo 0b1_0 * 0b1_0 . "\n";
echo -1_000 . "\n";
echo +1_000 . "\n";
echo 1_000.5 + 0.5 . "\n";
?>
--EXPECT--
3000
4000
3000
400
1
65536
4
-1000
1000
1001
--CLEAN--
<?php

