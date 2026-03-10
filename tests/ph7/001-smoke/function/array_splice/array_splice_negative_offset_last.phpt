--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with offset -1 removes last element
--FILE--
<?php
$a = array(10, 20, 30);
$r = array_splice($a, -1);
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
30
10,20
--CLEAN--
<?php
unset($a, $r);
