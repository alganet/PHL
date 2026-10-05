--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed trait constant's int default conflicts with a float one where the type keeps both
--DESCRIPTION--
For `int|float` php keeps the int an int, so int(3) and float(3.0) are not the
same value and the two constant definitions are incompatible.
--FILE--
<?php
trait TctuA { const int|float U = 3; }
class TctuC { use TctuA; const int|float U = 3.0; }
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TctuC and TctuA define the same constant (U) in the composition of TctuC. However, the definition differs and is considered incompatible. Class was composed in %s on line 3%A
