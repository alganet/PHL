--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Syntax error while declaring anonymous function
--FILE--
<?php
$f = function {
    echo "hello";
};
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "{", expecting "("%A
--CLEAN--
<?php
unset($f);
