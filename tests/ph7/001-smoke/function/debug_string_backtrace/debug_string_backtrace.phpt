--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_string_backtrace contains called function
--SKIPIF--
<?php
// PHL extension: `debug_string_backtrace()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: debug_string_backtrace() is not a php symbol'; }
?>
--FILE--
<?php
function foo() {
    $s = debug_string_backtrace();
    if (strpos($s, "Called function") !== false) {
        echo "contains_called\n";
    } else {
        echo "missing_called\n";
    }
}
foo();
?>
--EXPECT--
contains_called
--CLEAN--
<?php
unset($s);
