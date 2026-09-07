--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bare `static` inside a method is rejected too (it is a keyword, not a constant)
--SKIPIF--
<?php
// Both engines REJECT it, at different stages: php at PARSE time, PHL at RUNTIME as an
// undefined constant. Recorded in NEWPLAN section 7.
if (function_exists('zend_version')) echo 'skip php rejects bare static at parse time, PHL at runtime';
?>
--FILE--
<?php
class StaticBareWord {
    public function get() { return static; }
}
try {
    echo (new StaticBareWord)->get();
    echo "FAIL: static expanded to a value";
} catch (Error $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
Undefined constant "static"
--CLEAN--
<?php
