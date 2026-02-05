--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter with a callback should preserve keys and return only matching items
--FILE--
<?php
$in = array(1, 2, 3, 4, 5);
$out = array_filter($in, function($v) { return ($v % 2) === 0; });
// Expect keys 1 and 3 (0-based)
echo implode(',', array_keys($out)) . PHP_EOL;
// Empty result
$empty = array_filter(array(1, 3, 5), function($v) { return ($v % 2) === 0; });
echo count($empty) . PHP_EOL;
?>
--EXPECT--
1,3
0
--CLEAN--
<?php
unset($in, $out, $empty);
