--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test runtime error handling in VM
--FILE--
<?php
// Test that exercises VM runtime error handling
// This may trigger uncovered error formatting and throwing code

// Test with undefined function to trigger error
$result = nonexistent_function_12345();
echo "Test completed\n";
?>
--EXPECTF--
%AFatal error:%AUncaught Error: Call to undefined function nonexistent_function_12345()%A
--CLEAN--
<?php
unset($result);
