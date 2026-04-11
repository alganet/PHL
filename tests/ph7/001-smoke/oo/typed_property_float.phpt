--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: float
--FILE--
<?php
class TpRate {
    public float $value = 0.0;
}
$r = new TpRate();
$r->value = 1.5;
echo $r->value, "\n";
$r->value = $r->value * 2;
echo $r->value, "\n";
?>
--EXPECT--
1.5
3
--CLEAN--
<?php
unset($r);
