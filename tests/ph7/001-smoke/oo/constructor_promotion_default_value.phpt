--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: default value on promoted parameter
--FILE--
<?php
class CppDef {
    public function __construct(public int $n = 42, public string $s = "hi") {}
}
$a = new CppDef();
echo $a->n, "/", $a->s, "\n";
$b = new CppDef(7);
echo $b->n, "/", $b->s, "\n";
$c = new CppDef(1, "x");
echo $c->n, "/", $c->s, "\n";
?>
--EXPECT--
42/hi
7/hi
1/x
--CLEAN--
<?php
unset($a, $b, $c);
