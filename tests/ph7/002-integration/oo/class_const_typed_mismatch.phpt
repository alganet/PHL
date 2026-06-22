--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed class constant whose value violates its type is a definition-time fatal
--FILE--
<?php
class TypedConstMismatch {
    const int X = "bad";
}
echo TypedConstMismatch::X;
?>
--EXPECTF--
%s Fatal error:  Cannot use string as value for class constant TypedConstMismatch::X of type int %s
--CLEAN--
<?php
