--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed class constants: union and nullable types
--FILE--
<?php
class TypedConstUnion {
    const int|string A = "hello";
    const int|string B = 42;
    const ?int N = null;
    const ?int M = 7;
}
echo TypedConstUnion::A, "\n";
echo TypedConstUnion::B, "\n";
echo (TypedConstUnion::N === null) ? "isnull" : "notnull", "\n";
echo TypedConstUnion::M, "\n";
?>
--EXPECT--
hello
42
isnull
7
--CLEAN--
<?php
