--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait property's missing default does not hide a static against an instance property
--DESCRIPTION--
php compares a property's default only once the declarations agree:
visibility, static and readonly first. `public $p;` against
`public static $p = null;` both hold null and still conflict.
--FILE--
<?php
trait TpnStatic { public $p; }
class TpnStaticC { use TpnStatic; public static $p = null; }
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TpnStaticC and TpnStatic define the same property ($p) in the composition of TpnStaticC. However, the definition differs and is considered incompatible. Class was composed in %s on line 3%A
