--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Do-while loop
--FILE--
<?php
$i = 0;
$sum = 0;
do {
    $sum += $i;
    $i++;
} while ($i < 5);
echo $sum; // 10
?>
--EXPECT--
10
--CLEAN--
<?php
unset($i, $sum);
?>