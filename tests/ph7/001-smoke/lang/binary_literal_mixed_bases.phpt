--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal mixed with other numeric bases
--FILE--
<?php
echo (0b1010 + 0xA + 10 + 012) . "\n";
echo (0b1010 === 0xA) ? "true" : "false";
echo "\n";
echo (0b1010 === 012) ? "true" : "false";
echo "\n";
echo (0b1010 === 10) ? "true" : "false";
echo "\n";
echo (0b11111111 === 0xFF) ? "true" : "false";
echo "\n";
echo (0b11111111 === 0377) ? "true" : "false";
echo "\n";
echo (0b11111111 === 255) ? "true" : "false";
echo "\n";
?>
--EXPECT--
40
true
true
true
true
true
true
--CLEAN--
<?php

