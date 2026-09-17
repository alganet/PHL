--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
static without a variable is a syntax error naming the token and expecting "::" (was a bare skip freezing PHL's invented "Expected variable after 'static' keyword")

--FILE--
<?php
function foo() {
    static FOO;
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "FOO", expecting "::"%A
--CLEAN--
<?php

