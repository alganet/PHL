--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Direct named reference assignment (compile-time LHS)
--FILE--
<?php
$b = 2;
$a =& $b; // compile-time LHS assignment
$b = 5;
echo $a . "\n";
?>
--EXPECT--
5

--CLEAN--
<?php
unset($a, $b);
?>
