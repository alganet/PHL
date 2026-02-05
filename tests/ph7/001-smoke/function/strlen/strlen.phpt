--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strlen basic behaviors
--FILE--
<?php
echo strlen('hello') . "\n";           // 5
echo strlen('') . "\n";                // 0
echo strlen(' ') . "\n";               // 1 (single space)
echo strlen('hello world') . "\n";     // 11
echo strlen('12345') . "\n";           // 5
echo strlen('café') . "\n";            // 5 (with accent)
?>
--EXPECT--
5
0
1
11
5
5
--CLEAN--
<?php

