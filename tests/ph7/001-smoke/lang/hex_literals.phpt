--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test hex literals
--FILE--
<?php
echo 0xFF . "\n"; // 255
echo 0x10 . "\n"; // 16
echo 0x0 . "\n"; // 0
?>
--EXPECT--
255
16
0
--CLEAN--
<?php

