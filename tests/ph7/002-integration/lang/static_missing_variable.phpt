--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Missing variable name in static declaration
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function foo() {
    static FOO;
}
?>
--EXPECTF--
%s 3 Error:  Expected variable after 'static' keyword
Compile error
--CLEAN--
<?php

