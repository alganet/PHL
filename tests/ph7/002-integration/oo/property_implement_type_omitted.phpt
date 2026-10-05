--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed property answering an untyped interface property is refused
--DESCRIPTION--
An untyped interface property is not `mixed`: a typed property answering it
gets php's "must be omitted" sentence.
--FILE--
<?php
interface PimOmI { public $q { get; } }
class PimOmC implements PimOmI { public int $q = 0; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimOmC::$q must be omitted to match the parent definition in class PimOmI %s
