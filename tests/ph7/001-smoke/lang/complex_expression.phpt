--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex expression with nested ternary
--FILE--
<?php
$a = true;
$b = false;
$c = 10;
$d = 20;
$result = $a ? ($b ? $c : $d) : 0;
echo $result . "\n";
?>
--EXPECT--
20
--CLEAN--
<?php
unset($a, $b, $c, $d, $result);
