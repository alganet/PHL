--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var outside a class body is a parse error (was a bare skip asserting the PH7 extension that let it declare a plain variable)
--DESCRIPTION--
php adds ", expecting end of file" at top level and omits it inside a function;
PHL names the token without the expected-set tail, so this matches the body.
--FILE--
<?php
var $outside_var = "Hello World";
echo $outside_var . "\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "var"%A
--CLEAN--
<?php
