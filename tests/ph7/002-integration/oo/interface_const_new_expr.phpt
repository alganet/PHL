--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` expression cannot initialize an interface constant (PHP 8.5)
--FILE--
<?php
class X {}
interface I {
    const V = new X();
}
echo I::V;
?>
--EXPECTF--
%s Fatal error:  New expressions are not supported in this context%A
--CLEAN--
<?php
