--TEST--
Test compilation error edge cases to cover uncovered lines in compile.c
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test case that triggers line 190-192 in compile.c (PH7_CompilePHP function)
// This should cause compilation errors that hit the error paths

// Invalid syntax that triggers parsing errors
$a = ; // Missing value after assignment

// Invalid function declaration
function invalid_func(; // Missing parameter name

// Invalid array syntax
$arr = [key]; // Missing key-value pair

// Invalid string concatenation
$result = "hello" . ; // Missing second operand

// Invalid ternary operator
$value = true ? "yes" : ; // Missing false expression

// Invalid variable name
$$ = "invalid"; // Empty variable variable

// Invalid constant declaration
const INVALID_CONST = ; // Missing value

// Invalid switch statement
switch ($var) {
    case ; // Missing case value
        break;
}

// Invalid try-catch
try {
    throw new Exception("test");
} catch () { // Missing exception variable
}

// Invalid class declaration
class InvalidClass {
    public $prop = ; // Missing property value
}
?>
--EXPECTF--
%s 5 Error: '=': Missing/Invalid operand
%s 7 Error: Missing ')' after function 'invalid_func' signature
Compile error
