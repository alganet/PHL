--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 basic functionality
--FILE--
<?php
echo atan2(0, 0) . "\n";
echo atan2(1, 1) . "\n";
echo atan2(-1, -1) . "\n";
echo atan2(1, 0) . "\n";
echo atan2(-1, 0) . "\n";
echo atan2(0, 1) . "\n";
echo atan2(0, -1) . "\n";
echo is_float(atan2(1, 1)) ? "FLOAT" : "NOT_FLOAT";
echo "\n";
?>
--EXPECTF--
0
0.7853981633974%s
-2.356194490192%s
1.570796326794%s
-1.570796326794%s
0
3.141592653589%s
FLOAT
--CLEAN--
<?php

