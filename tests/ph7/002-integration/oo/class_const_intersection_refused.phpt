--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant declaring an INTERSECTION type still enforces it
--DESCRIPTION--
The other half of class_const_intersection_type.phpt: now that the type is
PARSED, the value has to satisfy it, and no constant expression can.
--FILE--
<?php
class CcrB { const Countable&ArrayAccess BAD = 1; }
echo "unreachable\n";
?>
--EXPECTF--
%ACannot use int as value for class constant CcrB::BAD of type Countable&ArrayAccess%A
--CLEAN--
<?php
