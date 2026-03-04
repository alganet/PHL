--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto label in different function (unreachable)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function baz() {
bar:
echo "hello";
}
function foo() {
goto bar;
}
?>
--EXPECTF--
%s 3 Warning:  Label 'bar' is defined but not referenced
%s 7 Error:  Label 'bar' is unreachable
Compile error
--CLEAN--
<?php

