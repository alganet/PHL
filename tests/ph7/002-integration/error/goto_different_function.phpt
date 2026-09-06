--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto label in different function
--FILE--
<?php
function foo() {
goto label;
}
label:
echo "hello";
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'label'%A
--CLEAN--
<?php

