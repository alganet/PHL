--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface CONSTRUCTOR constrains the implementor's, and both are rendered
--DESCRIPTION--
php exempts an INHERITED constructor from variance -- a child class may declare
whatever it likes -- but an interface that declares `__construct` constrains every
implementor's. PHL applied the inheritance exemption at both sites, and its
interface check counted parameters only, wording the two declarations as bare
`$name` lists.
--FILE--
<?php
interface DriI { public function __construct(int $a, ?string $b = null); }
class DriC implements DriI { public function __construct() {} }
echo "unreached\n";
?>
--EXPECTF--
%ADeclaration of DriC::__construct() must be compatible with DriI::__construct(int $a, ?string $b = null)%A
--CLEAN--
<?php
