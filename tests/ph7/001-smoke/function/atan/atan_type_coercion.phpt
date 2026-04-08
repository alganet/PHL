--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan type coercion with bool and numeric string
--FILE--
<?php
echo atan(true) . "\n";
echo atan(false) . "\n";
echo atan("1.5") . "\n";
?>
--EXPECTF--
0.7853981633974%s
0
0.9827937232473%s
--CLEAN--
<?php

