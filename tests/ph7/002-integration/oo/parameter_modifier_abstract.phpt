--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter takes no `abstract` modifier
--DESCRIPTION--
The other half of the same per-kind screen.
--FILE--
<?php
class PmaC { public function __construct(abstract public int $p) {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the abstract modifier on a parameter %s
--CLEAN--
<?php
