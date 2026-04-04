--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all with PREG_SET_ORDER
--FILE--
<?php
$m = null;
$r = preg_match_all('/(\w)(\w+)/', 'Hello World', $m, PREG_SET_ORDER);
echo $r . "\n";
echo $m[0][0] . "\n";
echo $m[0][1] . "\n";
echo $m[0][2] . "\n";
echo $m[1][0] . "\n";
echo $m[1][1] . "\n";
echo $m[1][2] . "\n";
?>
--EXPECT--
2
Hello
H
ello
World
W
orld
--CLEAN--
<?php

