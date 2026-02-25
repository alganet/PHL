--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with size larger than the array returns a single chunk equal to input
--FILE--
<?php
$in = array('x'=>1,'y'=>2);
$chunks = array_chunk($in, 10, true);
echo count($chunks) . PHP_EOL;
echo implode(',', array_keys($chunks[0]));
?>
--EXPECT--
1
x,y
--CLEAN--
<?php
unset($in, $chunks);
