--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array bucket growth test
--FILE--
<?php
// Test array operations that might trigger bucket growth
$a = array();
for ($i = 0; $i < 100; $i++) {
    $a["key$i"] = "value$i";
}
echo "Array created with " . count($a) . " elements\n";

// Test array_flip on a large array
$flipped = array_flip($a);
echo "Array flipped with " . count($flipped) . " elements\n";

// Test array_keys
$keys = array_keys($flipped);
echo "Got " . count($keys) . " keys\n";

echo "bucket_growth_ok\n";
?>
--EXPECT--
Array created with 100 elements
Array flipped with 100 elements
Got 100 keys
bucket_growth_ok
--CLEAN--
<?php
unset($a, $flipped, $keys);
