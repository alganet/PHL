--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override that widens the return type is rejected at compile time
--DESCRIPTION--
Ran PHL-only while the refusal named the two methods and not their SIGNATURES.
php renders both declarations (F36); it runs on either engine now.
--FILE--
<?php
class OvrP { public function f(): int { return 1; } }
class OvrC extends OvrP { public function f(): string { return "x"; } }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvrC::f(): string must be compatible with OvrP::f(): int%A
--CLEAN--
<?php
