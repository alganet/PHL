--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto to unreachable label results in compile-time error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
goto bar;

function foo() {
    bar:
    echo "This should not be reached";
}
?>
--EXPECTF--
%s %d Warning:  Label 'bar' is defined but not referenced
%s %d Error:  Label 'bar' is unreachable
Compile error
--CLEAN--
<?php

