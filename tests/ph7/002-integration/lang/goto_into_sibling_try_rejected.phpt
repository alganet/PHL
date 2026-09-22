--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto from one try into a SIBLING try is a PHL compile error — same nesting depth, different try (php allows it; PHL-pinned half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
try { echo "A\n"; goto lbl; } finally { echo "A fin\n"; }
try { echo "B\n"; lbl: throw new Exception("x"); }
catch (Exception $e) { echo "B caught: ", $e->getMessage(), "\n"; }
finally { echo "B fin\n"; }
echo "end\n";
?>
--EXPECTF--
%A'goto' into a try, catch or finally block is disallowed%A
--CLEAN--
<?php
