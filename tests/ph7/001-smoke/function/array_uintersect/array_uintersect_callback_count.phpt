--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should return the original array if only an array and callback are passed
--FILE--
<?php
$a = array(0 => 'a', 1 => 'b');
$c = array_uintersect($a, function($x, $y) { return strcmp($x, $y); });
echo count($c);
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a, $c);
