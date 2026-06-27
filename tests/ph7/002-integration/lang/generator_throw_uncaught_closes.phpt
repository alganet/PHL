--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() at a yield with no matching try closes the generator and propagates to the caller
--FILE--
<?php
// The injected exception is not caught inside the generator, so it propagates to
// the throw() call site (the caller's try) and the generator is left finished.
function g(){ yield 1; yield 2; }
$g = g();
echo "cur=".$g->current()."\n";
try { $g->throw(new RuntimeException("boom")); }
catch (RuntimeException $e) { echo "caller-caught:".get_class($e).":".$e->getMessage()."\n"; }
echo "valid=".var_export($g->valid(),true)."\n";
?>
--EXPECT--
cur=1
caller-caught:RuntimeException:boom
valid=false
--CLEAN--
<?php
