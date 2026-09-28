--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait method collision is reported against the composing class's line
--DESCRIPTION--
php names the line the composition was written on -- the `class` keyword's --
where PHL named the line the losing method was DECLARED on, which is inside
whichever trait the composition happened to reach second.
--FILE--
<?php
trait TmclX { public function m() { return 'X'; } }
trait TmclY { public function m() { return 'Y'; } }

class TmclClash
{
    use TmclX,
        TmclY;
}
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  Trait method TmclY::m has not been applied as TmclClash::m, because of collision with TmclX::m in %s on line 5%A
--CLEAN--
<?php
