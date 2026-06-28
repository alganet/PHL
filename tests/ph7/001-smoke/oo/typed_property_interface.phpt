--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property of interface type: accepts an implementer, rejects a non-implementer (TypeError)
--FILE--
<?php
interface IfeShape {}
class IfeCircle implements IfeShape {}
class IfeSquare {}
class IfeBox { public IfeShape $s; }
$b = new IfeBox();
$b->s = new IfeCircle();
echo "ok ", get_class($b->s), "\n";
try { $b->s = new IfeSquare(); }
catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after\n";
?>
--EXPECT--
ok IfeCircle
caught: Cannot assign IfeSquare to property IfeBox::$s of type IfeShape
after
--CLEAN--
<?php
unset($b);
