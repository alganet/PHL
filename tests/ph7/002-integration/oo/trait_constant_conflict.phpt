--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two traits defining the same constant with different definitions is a composition fatal
--DESCRIPTION--
php's rule for a constant is the one it already applies to a property: the name
being taken is only a conflict when the DEFINITION differs, and it names the
FIRST definition rather than the standing one.
--FILE--
<?php
trait TccA { const K = 1; }
trait TccB { const K = 2; }
class TccC { use TccA, TccB; }
echo "unreachable\n";
?>
--EXPECTF--
%ATccA and TccB define the same constant (K) in the composition of TccC. However, the definition differs and is considered incompatible. Class was composed%A
--CLEAN--
<?php
