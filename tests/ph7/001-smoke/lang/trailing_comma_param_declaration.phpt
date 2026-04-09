--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in function parameter declaration
--FILE--
<?php
function tcPdFoo($a,) { echo $a . "\n"; }
tcPdFoo(1);
function tcPdBar($a, $b,) { echo ($a + $b) . "\n"; }
tcPdBar(2, 3);
?>
--EXPECT--
1
5
--CLEAN--
<?php
