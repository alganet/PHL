--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error recovery paths to cover uncovered error handling
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test error recovery during compilation to cover uncovered paths
// in PH7_ErrorRecover and related error handling functions

// This should trigger error recovery paths
function test_error_recovery() {
    // Syntax error that should trigger recovery
    $invalid_syntax = ;
    echo "This should not execute\n";
}

// Call the function to trigger compilation error
test_error_recovery();
?>
--EXPECTF--
%s %d Error:  '=': Missing/Invalid operand
Compile error
--CLEAN--
<?php
unset($invalid_syntax);
