--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: weak-mode numeric-string coercion
--FILE--
<?php
class TpModel {
    public int $n = 0;
    public float $f = 0.0;
    public string $s = "";
}
$m = new TpModel();
$m->n = "42";
$m->f = "3.14";
$m->s = 99;
echo $m->n, " ", is_int($m->n) ? "int" : "?", "\n";
echo $m->f, " ", is_float($m->f) ? "float" : "?", "\n";
echo $m->s, " ", is_string($m->s) ? "string" : "?", "\n";
?>
--EXPECT--
42 int
3.14 float
99 string
--CLEAN--
<?php
unset($m);
