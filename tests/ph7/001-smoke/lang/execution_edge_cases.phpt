--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Execution edge cases for VM coverage
--FILE--
<?php
// Test various execution scenarios to cover VM edge cases in vm.c

// Test 1: Loop constructs with complex conditions
$count = 0;
for ($i = 0; $i < 100; $i += 2) {
    if ($count >= 50) break;
    if ($i % 3 == 0) {
        $count++;
        continue;
    }
    $count += 2;
}
echo "Loop execution: count=$count, i=$i\n";

// Test 2: Switch statement with various cases
$value = 17;
$result = "";
switch ($value % 5) {
    case 0:
        $result = "divisible_by_5";
        break;
    case 1:
    case 3:
        $result = "odd_remainder";
        break;
    case 2:
    case 4:
        $result = "even_remainder";
        break;
    default:
        $result = "unexpected";
}
echo "Switch result: $result\n";

// Test 3: Try-catch with different exception types
$exception_test = "no_exception";
try {
    if (false) {
        throw new Exception("Random exception");
    }
    $exception_test = "normal_execution";
} catch (Exception $e) {
    $exception_test = "caught_exception: " . $e->getMessage();
}
echo "Exception handling: $exception_test\n";

// Test 4: Recursive function calls
function fibonacci($n) {
    if ($n <= 1) return $n;
    return fibonacci($n - 1) + fibonacci($n - 2);
}
$fib_result = fibonacci(10);
echo "Recursion: fib(10) = $fib_result\n";

// Test 5: Variable scope and references
$global_var = "global";
function test_scope() {
    global $global_var;
    $local_var = "local";
    $ref_var = &$global_var;
    $ref_var = "modified_global";
    return $local_var;
}
$scope_result = test_scope();
echo "Scope test: $global_var, $scope_result\n";

// Test 6: Complex object operations
class ExecutionEdgeCasesTestClass {
    public $property = "initial";
    private $private_prop = "private";

    public function method($param) {
        return $this->property . "_" . $param;
    }

    public function __toString() {
        return "ExecutionEdgeCasesTestClass: " . $this->property;
    }
}

$obj = new ExecutionEdgeCasesTestClass();
$obj->property = "modified";
$method_result = $obj->method("test");
echo "Object operations: $method_result\n";

// Test 7: Array iteration with references
$array = array("a" => 1, "b" => 2, "c" => 3);
foreach ($array as $key => &$value) {
    $value *= 2;
}
$refs = array();
foreach ($array as $k => $v) {
    $refs[] = "$k=$v";
}
echo "Array iteration with refs: " . implode(" ", $refs) . "\n";

// Test 8: Dynamic function calls and variable variables
$func_name = "strtoupper";
$var_name = "dynamic_var";
$$var_name = "hello world";
$result = $func_name($$var_name);
echo "Dynamic operations: $result\n";

// Test 9: Include/require simulation with eval
$code = '$included_var = "included_value"; return $included_var;';
$included_result = eval($code);
echo "Eval execution: $included_result\n";

// Test 10: Complex type juggling
$int_val = 42;
$string_val = "123";
$bool_val = true;
$float_val = 3.14;

$mixed_ops = $int_val + $string_val * $bool_val - $float_val / 2;
echo "Type juggling: $mixed_ops\n";

echo "Execution edge cases test completed\n";
?>
--EXPECT--
Loop execution: count=50, i=60
Switch result: even_remainder
Exception handling: normal_execution
Recursion: fib(10) = 55
Scope test: modified_global, local
Object operations: modified_test
Array iteration with refs: a=2 b=4 c=6
Dynamic operations: HELLO WORLD
Eval execution: included_value
Type juggling: 163.43
Execution edge cases test completed
--CLEAN--
<?php
unset($count, $value, $result, $exception_test, $fib_result, $global_var, $local_var, $ref_var, $scope_result, $obj, $method_result, $array, $refs, $func_name, $var_name, $code, $included_result, $int_val, $string_val, $bool_val, $float_val, $mixed_ops);
