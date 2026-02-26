--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: very large count is handled (not artificially clamped)
--FILE--
<?php
// pick a count just above the old 1048576 limit used internally
$count = 1048577;
$a = array_fill(0, $count, 'x');
echo count($a) . "\n";
?>
--EXPECT--
1048577
--CLEAN--
<?php
unset($count, $a);
