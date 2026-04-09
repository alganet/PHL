--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in function call arguments
--FILE--
<?php
function tcFcAdd($a, $b, $c) { return $a + $b + $c; }
echo tcFcAdd(1, 2, 3,) . "\n";
echo tcFcAdd(10, 20, 30,) . "\n";
?>
--EXPECT--
6
60
--CLEAN--
<?php
