--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal basic values
--FILE--
<?php
echo 0b0 . "\n";
echo 0b1 . "\n";
echo 0b10 . "\n";
echo 0b11 . "\n";
echo 0b100 . "\n";
echo 0b1010 . "\n";
echo 0b11111111 . "\n";
echo 0B0 . "\n";
echo 0B1 . "\n";
echo 0B1010 . "\n";
?>
--EXPECT--
0
1
2
3
4
10
255
0
1
10
--CLEAN--
<?php

