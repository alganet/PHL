--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter takes no `static` modifier
--DESCRIPTION--
php has a per-kind sentence for the modifiers a PARAMETER never takes; PHL
answered its own "Invalid argument name", which describes the parser rather than
the declaration.
--FILE--
<?php
class PmsC { public function __construct(static public int $p) {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the static modifier on a parameter %s
--CLEAN--
<?php
