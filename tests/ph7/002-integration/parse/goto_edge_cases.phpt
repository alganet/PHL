--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto edge cases and cross-function label resolution
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test goto with undefined labels in various contexts to cover
// GenStateGetLabel error paths

function test_function() {
    // Cross-function goto - label doesn't exist in this scope
    goto NONEXISTENT_LABEL;
}

// Another function with goto to undefined label
function another_function() {
    goto MISSING_LABEL;
    echo "This should not execute\n";
}

// Goto in global scope to undefined label
goto GLOBAL_MISSING_LABEL;

// Nested function with goto to undefined label
function outer_function() {
    function inner_function() {
        goto INNER_MISSING;
    }

    inner_function();
}

outer_function();

?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'NONEXISTENT_LABEL'%AFatal error:%A'goto' to undefined label 'MISSING_LABEL'%AFatal error:%A'goto' to undefined label 'INNER_MISSING'%AFatal error:%A'goto' to undefined label 'GLOBAL_MISSING_LABEL'%A
--CLEAN--
<?php

