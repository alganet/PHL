--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice on array with string keys removes by position
--FILE--
<?php
$a = array('a' => 1, 'b' => 2, 'c' => 3);
$r = array_splice($a, 1, 1);
echo implode(',', $r) . "\n";
echo count($a);
?>
--EXPECT--
2
2
--CLEAN--
<?php
unset($a, $r);
