--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A finally-return swallow leaves no stale pending action or leaked try frame
--DESCRIPTION--
A `return` inside a finally entered VIA THE THROW REDIRECT (VmThrowInline
queued an FA_RETHROW and jumped into the finally body) short-circuits that
finally's OP_END_FINALLY, orphaning the queued action on pVm->aFinallyAction
and — in a generator — leaving the try's transparent frame above the body
frame. Three observable corruptions followed, each pinned here: an enclosing
function's next OP_END_FINALLY popped the orphan instead of its own action
(re-raising the swallowed exception / losing output); after such a GENERATOR
completed, its un-detached frames polluted the resumer's chain so the next
try at that scope recorded the wrong owner frame and its CAUGHT throw
silently ended the script (exit 0); and each orphan held a ref on the
swallowed exception instance. Activations now discard their own leftover
actions at every exit (record pop + exec finalize, never on suspend), and a
completing coroutine frees trailing exception wrappers like the suspend path.
--FILE--
<?php
function fsna_g1() { try { fsna_nosuchfn(); } finally { return; } yield 1; }
foreach (fsna_g1() as $fsna_v);
try { throw new Exception("p"); } catch (Exception $e) { echo "A:c "; }
echo "A:tail\n";
function fsna_g2() { try { throw new Exception("x"); } finally { return; } yield 1; }
foreach (fsna_g2() as $fsna_v);
try { throw new Exception("p"); } catch (Exception $e) { echo "B:c "; }
echo "B:tail\n";
function fsna_leak() { try { fsna_nosuchfn(); } finally { return 1; } }
try { echo "C:t "; } finally { fsna_leak(); echo "C:fin "; }
echo "C:tail\n";
function fsna_leak2() { try { fsna_nosuchfn2(); } finally { return 2; } }
try {
    try { throw new Exception("d"); } finally { fsna_leak2(); echo "D:fin "; }
} catch (Exception $e) { echo "D:c "; }
echo "D:tail\n";
function fsna_g3() { try { fsna_nosuchfn3(); } finally { return; } yield 99; }
$fsna_n = 0;
foreach (fsna_g3() as $fsna_v) { $fsna_n++; }
echo "E:", $fsna_n, "\n";
for ($fsna_i = 0; $fsna_i < 10000; $fsna_i++) {
    fsna_leak();
}
echo "F:loop-ok\n";
echo "end\n";
?>
--EXPECT--
A:c A:tail
B:c B:tail
C:t C:fin C:tail
D:fin D:c D:tail
E:0
F:loop-ok
end
--CLEAN--
<?php
unset($fsna_v, $fsna_n, $fsna_i);
