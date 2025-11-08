--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Loops
--FILE--
<?php
// For loop
$sum = 0;
for ($i = 1; $i <= 3; $i++) {
    $sum += $i;
}
echo $sum; // 6
echo "\n";

// While loop
$j = 1;
$prod = 1;
while ($j <= 3) {
    $prod *= $j;
    $j++;
}
echo $prod; // 6
?>
--EXPECT--
6
6
--CLEAN--
<?php
unset($sum, $i, $j, $prod);
?>