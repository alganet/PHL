--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test binary literals
--FILE--
<?php
echo 0b1010 . "\n"; // 10
echo 0b1111 . "\n"; // 15
echo 0b0 . "\n"; // 0
echo 0b1 . "\n"; // 1
?>
--EXPECT--
10
15
0
1
--CLEAN--
<?php

