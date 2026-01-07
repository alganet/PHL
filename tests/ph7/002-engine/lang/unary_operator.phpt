--CREDITS--
SPDX-FileCopyrightText: 2025 Test Coverage Contribution
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test coverage for unary plus/minus operator extraction
--FILE--
<?php
// This test triggers the uncovered line in ExprVerifyNodes where we need to 
// re-extract operators for unary plus/minus detection
// The expression starts with what could be interpreted as binary but should be unary

$result = +5;
echo "Result: " . $result . "\n";

$result2 = -10;
echo "Result2: " . $result2 . "\n";

$result3 = +"42";
echo "Result3: " . $result3 . "\n";
?>
--EXPECT--
Result: 5
Result2: -10
Result3: 42