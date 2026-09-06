--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto to unreachable label results in compile-time error
--FILE--
<?php
goto bar;

function foo() {
    bar:
    echo "This should not be reached";
}
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'bar'%A
--CLEAN--
<?php

