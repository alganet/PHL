--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int|null accepts null and int values
--FILE--
<?php
class TpUnNull {
    public int|null $n = null;
}
function tpunn_show($x) { echo is_null($x) ? "null" : "int:$x", "\n"; }
$o = new TpUnNull();
tpunn_show($o->n);
$o->n = 7;
tpunn_show($o->n);
$o->n = null;
tpunn_show($o->n);
?>
--EXPECT--
null
int:7
null
--CLEAN--
<?php
unset($o);
