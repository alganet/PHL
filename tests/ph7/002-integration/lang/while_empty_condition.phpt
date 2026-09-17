--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
while () is a syntax error on the ")" (was a bare skip freezing PHL's invented "Expected expression after 'while' keyword")

--FILE--
<?php
while () {
    echo "loop\n";
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ")"%A
--CLEAN--
<?php

