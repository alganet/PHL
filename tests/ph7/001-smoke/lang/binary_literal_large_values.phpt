--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal with large values
--FILE--
<?php
echo 0b11111111 . "\n";
echo 0b1111111111111111 . "\n";
echo 0b11111111111111111111111111111111 . "\n";
echo 0b100000000 . "\n";
echo 0b10000000000000000 . "\n";
?>
--EXPECT--
255
65535
4294967295
256
65536
--CLEAN--
<?php

