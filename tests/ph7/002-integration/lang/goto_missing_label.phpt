--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto a missing label results in compile-time error
--FILE--
<?php
goto missing_label;
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'missing_label'%A
--CLEAN--
<?php

