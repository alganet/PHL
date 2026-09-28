--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property cannot be both final and private
--DESCRIPTION--
A private property is invisible to a subclass, so `final` on one says nothing php
can honour. This is the PROPERTY rule only: php accepts the same pair on a
PROMOTED constructor parameter (`final private int $p` reflects as modifiers 36),
an asymmetry of php's own that the smoke twin pins.
--FILE--
<?php
class FppC { final private int $p = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Property cannot be both final and private %s
--CLEAN--
<?php
