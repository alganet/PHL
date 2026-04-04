--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all with PREG_PATTERN_ORDER (default)
--FILE--
<?php
$m = null;
$r = preg_match_all('/(\w)(\w+)/', 'Hello World', $m);
echo $r . "\n";
echo $m[0][0] . "\n";
echo $m[0][1] . "\n";
echo $m[1][0] . "\n";
echo $m[1][1] . "\n";
echo $m[2][0] . "\n";
echo $m[2][1] . "\n";
?>
--EXPECT--
2
Hello
World
H
W
ello
orld
--CLEAN--
<?php

