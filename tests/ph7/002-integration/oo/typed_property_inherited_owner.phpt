--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: inherited property error reports declaring class
--FILE--
<?php
class TpioThing {}
class TpioBase { public int $n = 0; }
class TpioChild extends TpioBase {}
$c = new TpioChild();
try {
    $c->n = new TpioThing();
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Cannot assign TpioThing to property TpioBase::$n of type int
--CLEAN--
<?php
unset($c);
