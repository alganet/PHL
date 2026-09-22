--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the void return check uses php's "method" noun inside a class body
--FILE--
<?php
class C {
    public function m(): void {
        return $this;
    }
}
?>
--EXPECTF--
%s Fatal error:  A void method must not return a value in %s
--CLEAN--
<?php
