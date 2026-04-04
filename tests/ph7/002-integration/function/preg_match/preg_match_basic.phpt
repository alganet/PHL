--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match basic matching with captures
--FILE--
<?php
$m = null;
$r = preg_match('/(\w+)\s(\w+)/', 'Hello World', $m);
echo $r . "\n";
echo $m[0] . "\n";
echo $m[1] . "\n";
echo $m[2] . "\n";
?>
--EXPECT--
1
Hello World
Hello
World
--CLEAN--
<?php

