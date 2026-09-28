--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`$this` may not be an assignment target
--DESCRIPTION--
The rule php makes in its ASSIGNMENT compiler -- which is exactly why a
read-modify-write (`$this++`, `$this += 1`) is NOT covered by it and fails at run
time on the operand types instead, and why the SOURCE of a `=&` is not covered
either.
--FILE--
<?php
class TatC { public function f() { $this = 1; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot re-assign $this %s
--CLEAN--
<?php
