--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the same bare return inside a class body is php's "method" noun
--FILE--
<?php
class C {
    public function bad(): string { return; }
}
?>
--EXPECTF--
%s Fatal error:  A method with return type must return a value in %s
--CLEAN--
<?php
