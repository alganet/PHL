--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in hexadecimal literals
--FILE--
<?php
echo 0xCAFE_F00D . "\n";
echo 0xFF_FF . "\n";
echo 0x1_2_3_4 . "\n";
echo 0xDEAD_BEEF . "\n";
echo 0X00_FF . "\n";
echo (int)(0xFF_FF === 0xFFFF) . "\n";
echo (int)(0xCAFE_F00D === 0xCAFEF00D) . "\n";
echo (int)(0x1_2_3_4 === 0x1234) . "\n";
?>
--EXPECT--
3405705229
65535
4660
3735928559
255
1
1
1
--CLEAN--
<?php

