--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe usable on $this inside a method body
--FILE--
<?php
class NsfOnThisC {
    public $val = 42;
    public function viaThis() { return $this?->val; }
}
$nsfOnThis_o = new NsfOnThisC();
echo $nsfOnThis_o->viaThis(), "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
unset($nsfOnThis_o);
