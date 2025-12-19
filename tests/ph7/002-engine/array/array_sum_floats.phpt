--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum() with floating point values
--FILE--
<?php
$array = array(1.5, 2.25, 3.75, 4.0);
$sum = array_sum($array);
echo "Sum: $sum\n";
echo ($sum === 11.5 ? 'true' : 'false') . "\n";
?>
--EXPECT--
Sum: 11.5
true