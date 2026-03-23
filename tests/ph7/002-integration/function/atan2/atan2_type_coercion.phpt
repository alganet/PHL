--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 type coercion with bool and numeric string
--FILE--
<?php
echo atan2(true, false) . "\n";
echo atan2(false, true) . "\n";
echo atan2("1.5", "2.5") . "\n";
echo atan2(1, 1.0) . "\n";
?>
--EXPECTF--
1.570796326794%s
0
0.5404195002705%s
0.7853981633974%s
--CLEAN--
<?php

