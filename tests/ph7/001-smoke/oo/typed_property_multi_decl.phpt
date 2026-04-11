--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: multiple declarations share the type
--FILE--
<?php
class TpVec {
    public int $a = 1, $b = 2, $c = 3;
}
$v = new TpVec();
echo $v->a + $v->b + $v->c, "\n";
$v->a = 10; $v->b = 20; $v->c = 30;
echo $v->a + $v->b + $v->c, "\n";
?>
--EXPECT--
6
60
--CLEAN--
<?php
unset($v);
