--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto unreachable label
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
bar:
function foo() {
    goto bar;
}
echo "ok";
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'bar'%A
--CLEAN--
<?php

