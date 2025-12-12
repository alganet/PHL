--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Verify compiler support for goto and label - forward and backward jumps
--FILE--
<?php
// Basic forward jump: 'skip' defined later
$out1 = 'first';
// jump over next assignment
goto skip;
$out1 = 'second';
skip:
echo $out1 . PHP_EOL; // expect 'first'
// Numeric forward/backward label flow
$i = 0;
// forward jump; $i should remain 0
goto L;
$i = 1; // Should be skipped
L:
echo $i . PHP_EOL; // expect '0'
?>
--EXPECT--
first
0
--CLEAN--
<?php
unset($out1, $i);
?>
