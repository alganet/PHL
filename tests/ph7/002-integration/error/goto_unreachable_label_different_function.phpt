--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto label in different function (unreachable)
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
%AFatal error:%A'goto' to undefined label 'bar'%A
--CLEAN--
<?php

