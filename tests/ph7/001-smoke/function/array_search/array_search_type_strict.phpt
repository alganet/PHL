--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search with strict type checking
--FILE--
<?php
// Test array_search with strict=true
$array = array(1, '1', 0, '0', null, '');

// Strict search for integer 1
$result = array_search(1, $array, true);
echo "Strict int 1: " . ($result === 0 ? "PASS" : "FAIL") . "\n";

// Strict search for string '1'
$result = array_search('1', $array, true);
echo "Strict string '1': " . ($result === 1 ? "PASS" : "FAIL") . "\n";

// Strict search for integer 0
$result = array_search(0, $array, true);
echo "Strict int 0: " . ($result === 2 ? "PASS" : "FAIL") . "\n";

// Strict search for string '0'
$result = array_search('0', $array, true);
echo "Strict string '0': " . ($result === 3 ? "PASS" : "FAIL") . "\n";

// Strict search for null
$result = array_search(null, $array, true);
echo "Strict null: " . ($result === 4 ? "PASS" : "FAIL") . "\n";

// Strict search for empty string (now distinct from null)
$result = array_search('', $array, true);
echo "Strict empty string: " . ($result === 5 ? "PASS" : "FAIL") . "\n";

// Strict search for non-existent value
$result = array_search(999, $array, true);
echo "Strict non-existent: " . ($result === false ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Strict int 1: PASS
Strict string '1': PASS
Strict int 0: PASS
Strict string '0': PASS
Strict null: PASS
Strict empty string: PASS
Strict non-existent: PASS
--CLEAN--
<?php
unset($array, $result);
