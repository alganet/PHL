--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Untyped class constants still parse: multi-declaration, expr value, interface const
--FILE--
<?php
class UntypedConstClass {
    const A = 1, B = 2;       // multi-declaration, no type
    const EXPR = 3 * 4;       // constant expression value
}
interface UntypedConstIface {
    const IV = 9;
}
class UntypedConstImpl implements UntypedConstIface {}
echo UntypedConstClass::A, UntypedConstClass::B, UntypedConstClass::EXPR, "\n";
echo UntypedConstImpl::IV, "\n";
?>
--EXPECT--
1212
9
--CLEAN--
<?php
