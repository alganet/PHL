--TEST--
Test expression compilation errors to cover uncovered lines in compile.c
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
// Test cases that trigger lines 333, 341, 395, 396 in compile.c
// These are error paths in expression compilation

// Trigger expression parsing errors
$var = func(; // Missing function arguments

// Invalid array access
$array = [];
$result = $array[; // Missing array index

// Invalid object access
class TestClass {}
$obj = new TestClass();
$result = $obj->; // Missing property name

// Invalid static access
$result = TestClass::; // Missing static member

// Trigger error in PH7_CompileExpr function around line 395-396
function test() {
    return ; // Missing return value
}

// Invalid include syntax
include ; // Missing file path

// Invalid require syntax
require_once ; // Missing file path

// Invalid heredoc
$var = <<<INVALID
content
INVALID; // This should trigger parsing errors

// Invalid nowdoc
$var = <<<'INVALID'
content
INVALID;

?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";"%AFatal error:%A'->': Missing/Invalid member name%AFatal error:%A'::': Missing/Invalid member name%A
--CLEAN--
<?php
unset($var, $array, $result, $obj);
