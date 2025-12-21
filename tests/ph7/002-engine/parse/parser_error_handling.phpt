--TEST--
Test parser error handling to cover uncovered lines in compile.c PH7_GenCompileError function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test cases that trigger lines 436, 445 in compile.c (PH7_GenCompileError function)
// These are error handling paths that generate compilation error messages

// Trigger various compilation errors to test error message generation

// Invalid goto usage - missing label
goto ; // Missing label name

// Invalid label syntax
: // Missing label name

// Duplicate labels
label1:
label1: // Duplicate label

// Invalid namespace usage
namespace ; // Missing namespace name

// Invalid use statement
use ; // Missing namespace/class name

// Invalid interface declaration
interface ; // Missing interface name

// Invalid trait usage
trait ; // Missing trait name

// Invalid abstract/final without class
abstract ; // Missing class keyword

final ; // Missing class keyword

// Invalid function parameter syntax
function test($param = ) { // Missing default value
}

// Invalid class constant
class Test {
    const CONSTANT = ; // Missing constant value
}

// Invalid method declaration
class Test {
    function method( { // Missing closing parenthesis
    }
}

// Invalid try-catch-finally
try {
} catch (Exception $e) {
} finally ; // Invalid finally syntax

?>
--EXPECTF--
%s 6 Error: goto: Invalid label name: ';'
%s 8 Error: Expected semi-colon ';' after 'goto' statement
%s 8 Error: Syntax error: Unexpected token ':'
%s 15 Notice: Namespace support is disabled in the current release of the PH7(2.1.4) engine
%s 17 Error: Syntax error: Unexpected keyword 'interface'
%s 21 Error: Syntax error: Unexpected keyword 'abstract'
%s 22 Error: Syntax error: Unexpected keyword 'final'
%s 24 Error: Missing argument default value
%s 28 Error: Empty constant 'CONSTANT' value
%s 32 Error: Missing ')' after method 'method' declaration
Compile error
