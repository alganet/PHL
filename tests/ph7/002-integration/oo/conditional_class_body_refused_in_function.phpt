--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class declared in a function body that is never called is still refused at compile time
--DESCRIPTION--
A method declared twice is the class compiler's refusal, so php reports it with
the file's compile, before the first statement and whether or not the function
holding the declaration is ever called.
--FILE--
<?php
echo "unreachable\n";
function ccbMake() {
    class CcbFnA {
        public function f() {}
        public function f() {}
    }
}
?>
--EXPECTF--
%ACannot redeclare CcbFnA::f() in %s on line 6%A
