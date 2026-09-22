--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a set hook returns void, so returning a value is the void compile error
--FILE--
<?php
class C {
    public $p { set { return 1; } }
}
?>
--EXPECTF--
%s Fatal error:  A void method must not return a value in %s
--CLEAN--
<?php
