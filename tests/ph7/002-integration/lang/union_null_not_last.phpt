--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A union with null in a non-trailing position (int|null|string) accepts every member
--FILE--
<?php
function un_f(int|null|string $x): string {
    return is_null($x) ? "null" : (is_int($x) ? "int" : "string");
}
echo un_f("abc"), "\n";
echo un_f(42), "\n";
echo un_f(null), "\n";

class UnBox {
    public int|null|string $v;
}
$b = new UnBox;
$b->v = "xyz"; echo $b->v, "\n";
$b->v = 9;     echo $b->v, "\n";
$b->v = null;  var_export($b->v); echo "\n";
?>
--EXPECT--
string
int
null
xyz
9
NULL
--CLEAN--
<?php
