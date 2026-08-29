--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare case-insensitive comparison

--FILE--
<?php
// Test case-insensitive comparison with 5th argument
$result1 = substr_compare('abcdef', 'ABC', 0, 3, true);
echo "Case-insensitive comparison: " . ($result1 === 0 ? "PASS" : "FAIL") . "\n";

// Test case-sensitive comparison (default)
$result2 = substr_compare('abcdef', 'ABC', 0, 3, false);
echo "Case-sensitive comparison: " . ($result2 !== 0 ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Case-insensitive comparison: PASS
Case-sensitive comparison: PASS
--CLEAN--
<?php
unset($result1, $result2);
