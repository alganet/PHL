--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break and continue outside loops and block nesting edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test break/continue outside loops to cover GenStateFetchBlock error paths

// Break outside any loop
break;

// Continue outside any loop
continue;

// Break with level outside any loop
break 2;

// Continue with level outside any loop
continue 3;

// Break in function outside loop
function test_break() {
    break;
}

// Continue in function outside loop
function test_continue() {
    continue;
}

// Nested functions with break/continue outside loops
function outer_func() {
    function inner_func() {
        break;
    }
    inner_func();
}

outer_func();

// Break in class method outside loop
class TestClass {
    public function test_method() {
        break;
    }
}

$obj = new TestClass();
$obj->test_method();

?>
--EXPECTF--
%s 4 Error: A 'break' statement may only be used within a loop or switch
%s 6 Error: A 'continue' statement may only be used within a loop or switch
%s 8 Error: A 'break' statement may only be used within a loop or switch
%s 10 Error: A 'continue' statement may only be used within a loop or switch
%s 13 Error: A 'break' statement may only be used within a loop or switch
%s 17 Error: A 'continue' statement may only be used within a loop or switch
%s 22 Error: A 'break' statement may only be used within a loop or switch
%s 30 Error: A 'break' statement may only be used within a loop or switch
Compile error