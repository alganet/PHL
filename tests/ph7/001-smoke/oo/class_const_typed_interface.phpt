--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed class constant declared on an interface is inherited by implementors
--FILE--
<?php
interface TypedConstIface {
    const int VERSION = 5;
}
class TypedConstImpl implements TypedConstIface {}
echo TypedConstIface::VERSION, "\n";
echo TypedConstImpl::VERSION, "\n";
?>
--EXPECT--
5
5
--CLEAN--
<?php
