--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dropping a generator suspended inside a try frees its exception-frame sub-chain (no leak/UAF)
--FILE--
<?php
// The generator suspends at a yield INSIDE a try, so an exception frame sits
// above its body frame. Dropping the only reference must free that frame
// sub-chain cleanly (ASan/leak guard for the suspended-in-try teardown path).
function g(){ try { yield 1; yield 2; } catch (Exception $e) {} }
$g = g();
$g->current();   // suspended inside the try
$g = null;       // release while suspended-in-try
echo "done\n";
?>
--EXPECT--
done
--CLEAN--
<?php
