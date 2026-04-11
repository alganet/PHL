--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: Foo|Bar class union accepts instances of either
--FILE--
<?php
class TpuFoo { public $tag = "F"; }
class TpuBar { public $tag = "B"; }
class TpuHolder {
    public TpuFoo|TpuBar $x;
}
$h = new TpuHolder();
$h->x = new TpuFoo();
echo $h->x->tag, "\n";
$h->x = new TpuBar();
echo $h->x->tag, "\n";
?>
--EXPECT--
F
B
--CLEAN--
<?php
unset($h);
