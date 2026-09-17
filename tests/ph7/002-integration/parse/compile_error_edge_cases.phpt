--TEST--
Test compilation error edge cases to cover uncovered lines in compile.c
--SKIPIF--
<?php
// php ABORTS at the first compile error; PHL keeps compiling and reports every one
// it finds (then "Error count limit reached" past 15). That is a deliberate engine
// difference, not a fidelity gap -- reporting the whole batch is more useful for an
// embedded engine -- so the two can never agree on this output. The FIRST error's
// text is what has to match php, and that is asserted by the single-error tests in
// this directory; this test exists to pin PHL's continuation behavior.
if (function_exists('zend_version')) { echo 'skip php aborts at the first compile error; PHL reports all (engine design)'; }
?>
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
%AParse error:%Asyntax error, unexpected token ";"%AParse error:%Asyntax error, unexpected token ";", expecting variable%A
--CLEAN--
<?php
unset($a, $arr, $result, $value);
