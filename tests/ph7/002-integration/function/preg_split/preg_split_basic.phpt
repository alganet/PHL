--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_split basic splitting and limit
--FILE--
<?php
$r = preg_split('/[\s,]+/', 'one, two, three four');
echo count($r) . "\n";
echo $r[0] . "\n";
echo $r[1] . "\n";
echo $r[2] . "\n";
echo $r[3] . "\n";

$r = preg_split('/[\s,]+/', 'one, two, three', 2);
echo count($r) . "\n";
echo $r[0] . "\n";
echo $r[1] . "\n";
?>
--EXPECT--
4
one
two
three
four
2
one
two, three
--CLEAN--
<?php

