--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class body inside an `if` that never runs is still refused before anything runs
--DESCRIPTION--
php compiles a conditional class's body with the file and only declares it when
its statement runs, so what its compiler refuses -- here `parent` in a class
with no base -- stops the whole file, even inside `if (false)`.
--FILE--
<?php
echo "unreachable\n";
if (false) {
    class CcbIfA { public function f() { return parent::f(); } }
}
?>
--EXPECTF--
%ACannot use "parent" when current class scope has no parent in %s on line 4%A
