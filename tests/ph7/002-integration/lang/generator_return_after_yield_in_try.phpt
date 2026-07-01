--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Regression lock: a generator that returns after a yield inside a try keeps its return value (the shape that reverted the prior inject attempt)
--FILE--
<?php
// A prior Generator::throw() inject attempt reconstructed the exception frame on
// pVm->pFrame at resume, which broke this common shape (lost return value / hang).
// The in-loop-inject design never touches pVm->pFrame at resume, so it must stay OK.

// return after a mid-expression yield inside a try/catch
function a() { try { $x = yield 1; } catch (Exception $e) {} return "ret:$x"; }
$g = a(); $g->current(); $g->send("V"); echo var_export($g->getReturn(), true), "\n";

// return straight out of a try (with finally) after a yield
function b() { try { yield 1; return "straight"; } finally { echo "fin\n"; } }
$g2 = b(); $g2->current(); $g2->next(); echo var_export($g2->getReturn(), true), "\n";

// value survives a try/finally around the yield, then is returned
function c() { try { $x = yield 1; } finally { echo "fin2\n"; } return "got:$x"; }
$g3 = c(); $g3->current(); $g3->send("HELLO"); echo var_export($g3->getReturn(), true), "\n";
?>
--EXPECT--
'ret:V'
fin
'straight'
fin2
'got:HELLO'
