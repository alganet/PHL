--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`$this` may not be the target of a reference bind
--DESCRIPTION--
The other half of the assignment rule. `$t =& $this` -- the same operator with
`$this` on the SOURCE side -- is legal php and binds the receiver by value.
--FILE--
<?php
class TrtC { public function f() { $x = 1; $this =& $x; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot re-assign $this %s
--CLEAN--
<?php
