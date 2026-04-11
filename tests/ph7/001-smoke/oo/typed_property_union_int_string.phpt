--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int|string union accepts both
--FILE--
<?php
class TpUnion {
    public int|string $v = 0;
}
function tpu_show($x) { echo is_int($x) ? "int:" : "str:", $x, "\n"; }
$o = new TpUnion();
tpu_show($o->v);
$o->v = 42;
tpu_show($o->v);
$o->v = "hello";
tpu_show($o->v);
?>
--EXPECT--
int:0
int:42
str:hello
--CLEAN--
<?php
unset($o);
