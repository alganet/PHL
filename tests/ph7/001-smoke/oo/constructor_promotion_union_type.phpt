--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: union type on promoted property
--FILE--
<?php
class CppUnion {
    public function __construct(public int|string $v) {}
}
$a = new CppUnion(42);
echo is_int($a->v) ? "int" : "str", ":", $a->v, "\n";
$b = new CppUnion("hello");
echo is_int($b->v) ? "int" : "str", ":", $b->v, "\n";
$a->v = "now string";
echo is_int($a->v) ? "int" : "str", ":", $a->v, "\n";
?>
--EXPECT--
int:42
str:hello
str:now string
--CLEAN--
<?php
unset($a, $b);
