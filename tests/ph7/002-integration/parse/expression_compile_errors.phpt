--TEST--
Test expression compilation errors to cover uncovered lines in compile.c
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
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
%s Error:  Syntax error,mismatched '(','[','{' or '?' %s
--CLEAN--
<?php
unset($var, $array, $result, $obj);
