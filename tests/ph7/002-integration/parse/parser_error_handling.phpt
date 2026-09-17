--TEST--
Test parser error handling to cover uncovered lines in compile.c PH7_GenCompileError function
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
%AParse error:%Asyntax error, unexpected token ";", expecting identifier%AFatal error:%AExpected semi-colon ';' after 'goto' statement%AFatal error:%ASyntax error: Unexpected token ':'%AFatal error:%ASyntax error: Unexpected keyword 'interface'%AFatal error:%ASyntax error: Unexpected keyword 'trait'%AFatal error:%ASyntax error: Unexpected keyword 'abstract'%AFatal error:%ASyntax error: Unexpected keyword 'final'%AFatal error:%AMissing argument default value%AFatal error:%AEmpty constant 'CONSTANT' value%AFatal error:%AMissing ')' after method 'method' declaration%A
--CLEAN--
<?php

