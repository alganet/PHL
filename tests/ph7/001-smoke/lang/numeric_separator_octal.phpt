--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in octal literals
--FILE--
<?php
echo 0_755 . "\n";
echo 07_55 . "\n";
echo 0_1_2_3 . "\n";
echo 0_777 . "\n";
echo (int)(0_755 === 0755) . "\n";
echo (int)(07_55 === 0755) . "\n";
echo (int)(0_1_2_3 === 0123) . "\n";
?>
--EXPECT--
493
493
83
511
1
1
1
--CLEAN--
<?php

