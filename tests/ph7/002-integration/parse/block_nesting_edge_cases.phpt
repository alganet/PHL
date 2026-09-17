--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break and continue outside loops and block nesting edge cases
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
%AFatal error:%A'break' not in the 'loop' or 'switch' context%AFatal error:%A'continue' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php
unset($obj);
