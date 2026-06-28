--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface return type: accepts an implementer, rejects a non-implementer (TypeError)
--FILE--
<?php
interface IreEmitter {}
class IreReal implements IreEmitter {}
class IreFake {}
function ire_make(bool $ok): IreEmitter { return $ok ? new IreReal() : new IreFake(); }
echo get_class(ire_make(true)), "\n";
try { ire_make(false); }
catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after\n";
?>
--EXPECT--
IreReal
caught: ire_make(): Return value must be of type IreEmitter, IreFake returned
after
