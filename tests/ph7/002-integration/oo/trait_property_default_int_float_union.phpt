--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait's int default against a class's float default conflicts where the type keeps both
--DESCRIPTION--
php converts an int default only for a type that admits float but not int. For
`int|float` the int stays an int, so int(1) and float(1.0) are not the same
value and the two definitions are incompatible.
--FILE--
<?php
trait TpdiUnion { public int|float $v = 1; }
class TpdiUnionC { use TpdiUnion; public int|float $v = 1.0; }
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TpdiUnionC and TpdiUnion define the same property ($v) in the composition of TpdiUnionC. However, the definition differs and is considered incompatible. Class was composed in %s on line 3%A
