--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf basic functionality
--FILE--
<?php
echo printf("Hello") . PHP_EOL;
echo printf("Number: %d", 42) . PHP_EOL;
echo printf("Float: %.2f", 3.14159) . PHP_EOL;
echo printf("String: %s", "test") . PHP_EOL;
echo printf("Hex: %x", 255) . PHP_EOL;
echo printf("Octal: %o", 8) . PHP_EOL;
?>
--EXPECT--
Hello5
Number: 4210
Float: 3.1411
String: test12
Hex: ff7
Octal: 109
--CLEAN--
<?php

