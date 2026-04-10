--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator basic values
--FILE--
<?php
echo 1_000 . "\n";
echo 1_000_000 . "\n";
echo 1_2_3 . "\n";
echo 100_000_000 . "\n";
echo 1_2 . "\n";
echo 999_999_999 . "\n";
echo (int)(1_000 === 1000) . "\n";
echo (int)(1_000_000 === 1000000) . "\n";
echo (int)(1_2_3 === 123) . "\n";
?>
--EXPECT--
1000
1000000
123
100000000
12
999999999
1
1
1
--CLEAN--
<?php

