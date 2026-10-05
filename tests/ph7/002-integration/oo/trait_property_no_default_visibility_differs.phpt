--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait property's missing default does not hide a different visibility
--DESCRIPTION--
php compares a property's default only once the declarations agree.
`public $p;` against `protected $p = null;` both hold null and still
conflict on the visibility.
--FILE--
<?php
trait TpnVis { public $p; }
class TpnVisC { use TpnVis; protected $p = null; }
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TpnVisC and TpnVis define the same property ($p) in the composition of TpnVisC. However, the definition differs and is considered incompatible. Class was composed in %s on line 3%A
