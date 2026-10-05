--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A syntax error inside a conditional class body is a parse error of the whole file
--FILE--
<?php
echo "unreachable\n";
if (PHP_VERSION_ID < 0) {
    class CcbSynA {
        public function f() { $x = ; }
    }
}
?>
--EXPECTF--
%Asyntax error, unexpected token ";" in %s on line 5%A
