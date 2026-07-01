--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` expression cannot be a property default value (PHP 8.5)
--FILE--
<?php
class X {}
class C {
    public $p = new X();
}
$c = new C();
echo get_class($c->p);
?>
--EXPECTF--
%s Fatal error:  New expressions are not supported in this context%A
--CLEAN--
<?php
