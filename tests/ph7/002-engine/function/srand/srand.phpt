--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: srand basic functionality
--FILE--
<?php
// Test basic srand functionality
// srand seeds the random number generator, returns nothing

// Test srand with no arguments (should not crash)
srand();
echo "NO_ARGS_OK\n";

// Test srand with seed value
srand(12345);
echo "SEED_OK\n";

// Test that rand() still works after seeding
$value1 = rand();
echo is_int($value1) ? "RAND_INT_OK\n" : "RAND_INT_FAIL\n";

// Test mt_srand (alias)
mt_srand(67890);
echo "MT_SEED_OK\n";

// Test that mt_rand() still works after mt_srand
$value2 = mt_rand();
echo is_int($value2) ? "MT_RAND_INT_OK\n" : "MT_RAND_INT_FAIL\n";

// Test srand with 0 seed
srand(0);
echo "ZERO_SEED_OK\n";
?>
--EXPECT--
NO_ARGS_OK
SEED_OK
RAND_INT_OK
MT_SEED_OK
MT_RAND_INT_OK
ZERO_SEED_OK
