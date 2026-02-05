--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array equality (==) and identity (===) should differ when types differ
--FILE--
<?php
$a = array('a' => 1);
$b = array('a' => '1');

echo ($a == $b) ? '1' : '0';
echo PHP_EOL;
echo ($a === $b) ? '1' : '0';
echo PHP_EOL;
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($a, $b);
