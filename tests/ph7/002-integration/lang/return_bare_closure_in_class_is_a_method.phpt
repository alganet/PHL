--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the noun follows the LEXICAL class scope, so a closure in a method is a "method"
--FILE--
<?php
class C {
    public function m() {
        return function (): int { return; };
    }
}
?>
--EXPECTF--
%s Fatal error:  A method with return type must return a value in %s
--CLEAN--
<?php
