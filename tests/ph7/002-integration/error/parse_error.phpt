--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Parser reports a parse error for malformed code
--FILE--
<?php
function broken( { // syntax error: missing ) brace
    echo "should never print\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "{", expecting variable%A
--CLEAN--
<?php

