--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
in_array should respect strict parameter and types
--FILE--
<?php
$a = array(1);
// Non-strict comparison should find '1' in array
echo in_array('1', $a) ? '1' : '0';
echo PHP_EOL;
// Strict comparison should not
echo in_array('1', $a, true) ? '1' : '0';
echo PHP_EOL;
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($a);
?>
