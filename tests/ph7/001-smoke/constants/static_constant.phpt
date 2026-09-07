--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`static` is a keyword, not a constant: a bare `static` is rejected
--SKIPIF--
<?php
// Both engines REJECT a bare `static`, at different stages: php at PARSE time
// ("syntax error, unexpected token ..., expecting \"::\""), PHL at RUNTIME as an
// undefined constant. Recorded in NEWPLAN section 7.
if (function_exists('zend_version')) echo 'skip php rejects bare static at parse time, PHL at runtime';
?>
--FILE--
<?php
// Caught: the smoke tier shares one interpreter, and an uncaught fatal both bails the
// run and poisons the process exit status.
try {
    echo static;
    echo "FAIL: static expanded to a value";
} catch (Error $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
Undefined constant "static"
--CLEAN--
<?php
