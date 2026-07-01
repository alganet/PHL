--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` expression cannot initialize a class constant (PHP 8.5), even nested
--FILE--
<?php
class X {}
class C {
    const V = [new X()];
}
echo C::V[0];
?>
--EXPECTF--
%s Fatal error:  New expressions are not supported in this context%A
--CLEAN--
<?php
