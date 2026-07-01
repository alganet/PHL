--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` expression cannot be a static property default value (PHP 8.5)
--FILE--
<?php
class X {}
class C {
    public static $p = new X();
}
echo get_class(C::$p);
?>
--EXPECTF--
%s Fatal error:  New expressions are not supported in this context%A
--CLEAN--
<?php
