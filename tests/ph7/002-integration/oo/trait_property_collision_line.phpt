--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An incompatible trait property names the line the class was composed on
--DESCRIPTION--
php ends this message with a clause of its own -- "Class was composed" -- and
lets the fatal's own " in %s on line %d" finish the sentence, so the line is the
composing class's. PHL stopped at "incompatible" and pointed at the trait.
--FILE--
<?php
trait TpclX { public $p = 1; }
trait TpclY { public $p = 2; }

class TpclClash
{
    use TpclX,
        TpclY;
}
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TpclX and TpclY define the same property ($p) in the composition of TpclClash. However, the definition differs and is considered incompatible. Class was composed in %s on line 5%A
--CLEAN--
<?php
