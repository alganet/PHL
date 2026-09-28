--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break and continue outside loops and block nesting edge cases
--SKIPIF--
<?php
// Both engines stop at this one refusal and word it identically; php prints a
// `Stack trace:` block under a compile-time FATAL that this engine does not
// (ECOSYSTEM.md F30, behind F6's frame attribution), so the message is pinned
// under PHL alone until that lands.
if (function_exists('zend_version')) { echo 'skip php prints a Stack trace under a compile-time fatal'; }
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
%AFatal error:%A'break' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php
unset($obj);
