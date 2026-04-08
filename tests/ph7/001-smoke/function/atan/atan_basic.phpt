--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan basic functionality
--FILE--
<?php
echo atan(0) . "\n";
echo atan(1) . "\n";
echo atan(-1) . "\n";
echo is_float(atan(0)) ? "FLOAT" : "NOT_FLOAT";
echo "\n";
?>
--EXPECTF--
0
0.7853981633974%s
-0.7853981633974%s
FLOAT
--CLEAN--
<?php

