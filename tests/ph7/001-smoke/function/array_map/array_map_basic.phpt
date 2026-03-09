--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map applies callback to each element of a numeric array
--FILE--
<?php
$r = array_map(function($v) { return $v * 2; }, array(10, 20, 30));
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
?>
--EXPECT--
20
40
60
--CLEAN--
<?php
unset($r);
