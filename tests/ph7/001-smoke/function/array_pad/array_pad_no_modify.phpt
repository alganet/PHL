--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad does not modify the original array
--FILE--
<?php
$a = array(1, 2);
$r = array_pad($a, 5, 'x');
echo count($a) . PHP_EOL;
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a, $r);
