--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait property's missing default does not hide a different declared type
--DESCRIPTION--
php compares a property's default only once the declarations agree. An
untyped `public $p;` against `public ?int $p = null;` both hold null, but
the type differs, so the two definitions are incompatible.
--FILE--
<?php
trait TpnType { public $p; }
class TpnTypeC { use TpnType; public ?int $p = null; }
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  TpnTypeC and TpnType define the same property ($p) in the composition of TpnTypeC. However, the definition differs and is considered incompatible. Class was composed in %s on line 3%A
