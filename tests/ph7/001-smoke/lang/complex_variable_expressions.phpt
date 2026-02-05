--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex Variable Expressions Test
--DESCRIPTION--
Test parsing of complex variable expressions with nested operations
--FILE--
<?php
// Test complex variable expressions that trigger different compilation paths
$a = array(1, 2, 3);
${'var'} = 'test';
$${'var'} = 42;

echo "Simple array access: " . $a[0] . "\n";
echo "Variable variable: " . ${'var'} . "\n";
echo "Nested variable variable: " . $$var . "\n";

// Complex expression with multiple operations
$result = ($a[0] + $$var) * 2;
echo "Complex expression result: $result\n";

// Test with function calls and variable variables
function test_func() {
    return 'func_result';
}

$func_name = 'test_func';
$call_func = 'test_func';
echo "Function variable call: " . $call_func() . "\n";

echo "Test completed successfully\n";
?>
--EXPECT--
Simple array access: 1
Variable variable: test
Nested variable variable: 42
Complex expression result: 86
Function variable call: func_result
Test completed successfully
--CLEAN--
<?php
unset($a, ${'var'}, $${'var'}, $result, $func_name, $call_func);
