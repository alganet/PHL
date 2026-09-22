--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a bare return in a typed property's get hook is the same compile error
--FILE--
<?php
class C {
    public int $p { get { return; } }
}
?>
--EXPECTF--
%s Fatal error:  A method with return type must return a value in %s
--CLEAN--
<?php
