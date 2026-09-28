--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two access-type modifiers on one member
--DESCRIPTION--
php says the same thing for `public public`, `public private` and two `(set)`
visibilities: they are all one modifier written twice.
--FILE--
<?php
class MmdaC { public private $p = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple access type modifiers are not allowed %s
--CLEAN--
<?php
