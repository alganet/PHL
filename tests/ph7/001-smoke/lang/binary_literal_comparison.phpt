--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal comparisons
--FILE--
<?php
echo (0b1010 === 10) ? "true" : "false";
echo "\n";
echo (0b1010 == 10) ? "true" : "false";
echo "\n";
echo (0b1010 > 0b1001) ? "true" : "false";
echo "\n";
echo (0b1010 < 0b1011) ? "true" : "false";
echo "\n";
echo (0b1010 >= 0b1010) ? "true" : "false";
echo "\n";
echo (0b1010 <= 0b1010) ? "true" : "false";
echo "\n";
echo (0b1010 !== 0b1011) ? "true" : "false";
echo "\n";
echo (0b1010 === 0xA) ? "true" : "false";
echo "\n";
echo (0b1010 === 012) ? "true" : "false";
echo "\n";
?>
--EXPECT--
true
true
true
true
true
true
true
true
true
--CLEAN--
<?php

