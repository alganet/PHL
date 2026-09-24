--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_print_backtrace prints php's own trace frames
--FILE--
<?php
// This used to assert PH7's `[Called function: foo]` line and was skipped under
// php for exactly that reason; the format is php's now, so both engines answer
// the same thing and the skip is gone.
function dpb_foo() {
    ob_start();
    debug_print_backtrace();
    $dpb_lines = explode("\n", trim(str_replace(__FILE__, 'FILE', ob_get_clean())));
    // Only this file's own frame is asserted: whatever ran the file sits under it.
    return implode("\n", array_values(array_filter($dpb_lines,
        static fn($dpb_l) => strpos($dpb_l, ': dpb_foo(') !== false))) . "\n";
}
echo dpb_foo();
?>
--EXPECT--
#0 FILE(13): dpb_foo()
