--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw leaving a callee through an OPEN try unwinds that callee's frame too
--FILE--
<?php
/* A throw that leaves a function through a `try` the function still had OPEN
 * never reaches that try's landing pad, so the try's transparent frame is still
 * stacked on the function's own. Both have to come off. And a redirect to a
 * catch/finally belongs to ONE activation: two live activations of the same
 * function share their bytecode, so the owner is the frame, not the code. */

// (1) A callee whose non-matching try is open when the throw passes through.
function gtoInner() { throw new Exception('boom'); }
function gtoOuter() {
    try { return gtoInner(); }
    catch (BadFunctionCallException $e) { return null; }   // never matches
}
function gtoGen() { yield gtoOuter(); }
try { foreach (gtoGen() as $v) { echo "unreachable"; } }
catch (Exception $e) { echo "1:", $e->getMessage(), "\n"; }

// (2) The engine is still sane afterwards: more generators, more locals.
function gtoAfter() { yield 1; yield 2; }
foreach (gtoAfter() as $v) { echo "2:$v "; }
echo "\n";

// (3) Two nested open tries, neither matching.
function gtoTwo() {
    try { try { gtoInner(); } catch (LogicException $e) { echo "no"; } }
    catch (BadFunctionCallException $e) { echo "no"; }
}
function gtoGen2() { yield gtoTwo(); }
try { foreach (gtoGen2() as $v) {} }
catch (Exception $e) { echo "3:", $e->getMessage(), "\n"; }

// (4) Sibling activations of ONE generator function: the finally that runs is
// the one whose try was actually entered, in ITS variable scope.
function gtoBlock($name) {
    if ($name === 'foo') {
        try {
            $level = 7;
            foreach (gtoBody() as $d) { yield $d; }
        } catch (Throwable $e) {
            throw $e;
        } finally {
            echo "4:fin name=$name level=", $level ?? 'unset', "\n";
        }
    } else {
        throw new RuntimeException("no block \"$name\"");
    }
}
function gtoBody() { yield from gtoBlock('unknown'); }
try { foreach (gtoBlock('foo') as $v) { echo "unreachable"; } }
catch (Throwable $e) { echo "4:", $e->getMessage(), "\n"; }

// (5) The same, with a catch instead of a finally: the sibling activation that
// owns the try is the one that catches.
function gtoCatch($name) {
    if ($name === 'foo') {
        try { foreach (gtoCatchBody() as $d) { yield $d; } }
        catch (RuntimeException $e) { echo "5:caught in name=$name\n"; }
    } else {
        throw new RuntimeException('inner');
    }
}
function gtoCatchBody() { yield from gtoCatch('unknown'); }
foreach (gtoCatch('foo') as $v) { echo "unreachable"; }

// (6) An output buffer opened inside the try is still torn down by the finally.
function gtoOb($name) {
    if ($name === 'foo') {
        try { $lvl = ob_get_level(); ob_start(); foreach (gtoObBody() as $d) yield $d; }
        finally { while (ob_get_level() > $lvl) { ob_end_clean(); } }
    } else {
        throw new RuntimeException('ob');
    }
}
function gtoObBody() { yield from gtoOb('unknown'); }
$lvl0 = ob_get_level();
try { foreach (gtoOb('foo') as $v) {} } catch (Throwable $e) {}
echo "6:", ob_get_level() === $lvl0 ? 'balanced' : 'leaked', "\n";
?>
--EXPECT--
1:boom
2:1 2:2 
3:boom
4:fin name=foo level=7
4:no block "unknown"
5:caught in name=foo
6:balanced
