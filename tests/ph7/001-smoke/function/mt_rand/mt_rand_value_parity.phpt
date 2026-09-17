--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mt_srand/mt_rand reproduce php's exact MT19937 values across all range widths
--FILE--
<?php
// Bare mt_rand() after mt_srand() reproduces php's exact 31-bit sequence.
mt_srand(1234);
for ($i = 0; $i < 6; $i++) {
    echo mt_rand(), "\n";
}

// A range that fits in 32 bits.
echo "ranged:";
mt_srand(2024);
for ($i = 0; $i < 10; $i++) {
    echo " ", mt_rand(0, 100);
}
echo "\n";

// Ranges wider than 32 bits take php's 64-bit two-draw path.
echo "wide:";
mt_srand(9);
echo " ", mt_rand(-9000000000, 9000000000);
echo " ", mt_rand(0, PHP_INT_MAX);
echo "\n";

// mt_rand() is strict about a reversed range (unlike rand()).
try {
    mt_rand(5, 1);
} catch (\ValueError $e) {
    echo "err:", $e->getMessage(), "\n";
}

echo "mtmax:", mt_getrandmax(), "\n";
?>
--EXPECT--
411284887
1068724585
1335968403
1756294682
940013158
1314500282
ranged: 31 35 85 55 69 52 16 42 60 22
wide: -3016090308 9207330117116418686
err:mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)
mtmax:2147483647
