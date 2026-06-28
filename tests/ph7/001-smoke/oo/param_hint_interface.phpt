--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface/abstract parameter hint accepts a valid implementer/subclass (positional + named)
--FILE--
<?php
interface IphReader {}
class IphFile implements IphReader {}
abstract class IphBase {}
class IphChild extends IphBase {}
function iph_take(IphReader $r): string { return "ok:" . get_class($r); }
function iph_named(IphReader $r): string { return "named:" . get_class($r); }
function iph_abs(IphBase $b): string { return "abs:" . get_class($b); }
echo iph_take(new IphFile()), "\n";
echo iph_named(r: new IphFile()), "\n";
echo iph_abs(new IphChild()), "\n";
?>
--EXPECT--
ok:IphFile
named:IphFile
abs:IphChild
