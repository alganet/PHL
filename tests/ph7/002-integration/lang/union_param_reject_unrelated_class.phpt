--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union parameter: Foo|Bar rejects unrelated class instance
--FILE--
<?php
class UpcFoo {}
class UpcBar {}
class UpcBaz {}
function upruc_f(UpcFoo|UpcBar $x) {}
try {
    upruc_f(new UpcBaz());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
upruc_f(): Argument #1 ($x) must be of type UpcFoo|UpcBar, UpcBaz given%A
--CLEAN--
<?php
