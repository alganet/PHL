--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
srand seeds a reproducible MT19937 stream with php-exact values
--FILE--
<?php
// A fixed seed must reproduce php's exact rand() sequence (MT19937), and must
// give the SAME sequence every time it is re-seeded (the whole point of srand).
srand(42);
$a = [];
for ($i = 0; $i < 6; $i++) {
    $a[] = rand();
}
echo implode("\n", $a), "\n";
echo "pass2\n";
srand(42);
$b = [];
for ($i = 0; $i < 6; $i++) {
    $b[] = rand();
}
echo ($a === $b ? "reproducible" : "DIVERGED"), "\n";

// Ranged draws are also value-exact under a seed.
echo "ranged:";
srand(100);
for ($i = 0; $i < 10; $i++) {
    echo " ", rand(1, 6);
}
echo "\n";

// A negative seed is truncated to 32 bits like php.
srand(-7);
echo "neg:", rand(), "\n";

// rand() swaps a reversed range instead of throwing (php compat quirk).
srand(1);
echo "swap:", rand(9, 2), "\n";

echo "max:", getrandmax(), "\n";
?>
--EXPECT--
804318771
1710563033
2041643438
393923207
1571945013
1674373667
pass2
reproducible
ranged: 5 5 6 6 2 2 1 5 1 3
neg:119565662
swap:7
max:2147483647
--CLEAN--
<?php
unset($a, $b);
