--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A signed bare float literal (-1.0) cannot initialize an int typed class constant
--FILE--
<?php
class C {
    const int X = -1.0;
}
echo C::X;
?>
--EXPECTF--
%s Fatal error:  Cannot use float as value for class constant C::X of type int %s
--CLEAN--
<?php
