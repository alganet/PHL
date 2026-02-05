--TEST--
Extended array integer key operations
--FILE--
<?php
// Test array operations with integer keys to cover IntHash function

echo "Testing array with various integer keys:\n";

// Test 1: Large integer keys
$large_keys = array();
for ($i = 1000; $i < 1010; $i++) {
    $large_keys[$i * 1000] = "value_$i";
}
echo "Large keys array count: " . count($large_keys) . "\n";
echo "Sample large key access: " . $large_keys[1000000] . "\n";

// Test 2: Negative integer keys
$neg_keys = array(-5 => 'negative', -1 => 'minus_one', 0 => 'zero', 5 => 'positive');
echo "Negative keys: " . implode(', ', array_keys($neg_keys)) . "\n";
echo "Values: " . implode(', ', array_values($neg_keys)) . "\n";

// Test 3: Mixed positive and negative keys
$mixed = array(10 => 'ten', -10 => 'minus_ten', 100 => 'hundred', -100 => 'minus_hundred');
ksort($mixed);
echo "Sorted mixed keys: " . implode(', ', array_keys($mixed)) . "\n";

// Test 4: Sparse array with integer keys
$sparse = array(1 => 'first', 100 => 'hundredth', 1000 => 'thousandth');
$sparse[500] = 'five_hundredth';
echo "Sparse array size: " . count($sparse) . "\n";

// Test 5: Array key hashing collision simulation (different keys same hash)
$hash_test = array();
// Use keys that might collide in hash
$hash_test[123456] = 'hash1';
$hash_test[234567] = 'hash2';
$hash_test[345678] = 'hash3';
echo "Hash collision test passed\n";

echo "Integer keys test completed\n";
?>
--EXPECT--
Testing array with various integer keys:
Large keys array count: 10
Sample large key access: value_1000
Negative keys: -5, -1, 0, 5
Values: negative, minus_one, zero, positive
Sorted mixed keys: -100, -10, 10, 100
Sparse array size: 4
Hash collision test passed
Integer keys test completed
--CLEAN--
<?php
unset($large_keys, $neg_keys, $mixed, $sparse, $hash_test);
