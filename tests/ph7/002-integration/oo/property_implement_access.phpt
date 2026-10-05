--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering an interface must be public
--DESCRIPTION--
Interface properties are public, and a narrower one is refused with the
access-level sentence php uses for a redeclaration.
--FILE--
<?php
interface PimAcI { public int $p { get; } }
class PimAcC implements PimAcI { protected int $p = 0; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Access level to PimAcC::$p must be public (as in class PimAcI) %s
