--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: used inside a ternary expression
--FILE--
<?php
$flag = true;
$v = 2;
$r = $flag ? match ($v) { 1 => 'one', 2 => 'two' } : 'off';
echo $r, "\n";
$flag = false;
$r2 = $flag ? match ($v) { 1 => 'one', 2 => 'two' } : 'off';
echo $r2, "\n";
?>
--EXPECT--
two
off
--CLEAN--
<?php
unset($flag, $v, $r, $r2);
