--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Parser reports a parse error for malformed code
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
function broken( { // syntax error: missing ) brace
    echo "should never print\n";
?>
--EXPECTF--
%s Fatal error:  Missing ')' after function 'broken' signature %s
--CLEAN--
<?php

