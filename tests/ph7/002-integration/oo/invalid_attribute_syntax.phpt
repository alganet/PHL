--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a stray token after a property name is a syntax error expecting "," or ";" (was a bare skip freezing PHL's invented "Expected '=' or ';' after attribute name")

--FILE--
<?php
class TestClass {
    public $var 123;
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting "," or ";"%A
--CLEAN--
<?php

