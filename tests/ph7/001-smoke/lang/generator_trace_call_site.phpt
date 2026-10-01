--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator's own trace frame names the site that RESUMED it, and its receiver
--FILE--
<?php
/* A coroutine's body frame is built DETACHED and never goes through the
 * frame-entry path that records a call site, so a backtrace taken inside one
 * reported the frame BELOW it at line 0 — printed as line 1, in whatever file
 * the include stack happened to top out at. php names the site the generator is
 * being RESUMED from, not where it was created: the `foreach` for the first
 * step and each one after it, the `yield from` line for a delegate. (A resume
 * driven by an explicit ->current()/->next() call is a different shape
 * instead — php interposes a frame for the internal method and leaves the body
 * frame locationless — so those spellings are not pinned here.) */
function gtsShow($tag)
{
    $out = $tag;
    foreach ((new Exception())->getTrace() as $f) {
        /* Frames outside this file belong to whatever ran it (the .phpt runner's
         * own include), so leave them out and the pin stays portable. */
        if (($f['file'] ?? null) !== __FILE__) { continue; }
        $out .= ' ' . $f['line'] . '/' . $f['function'];
    }
    echo $out, "\n";
}

function gtsPlain() { gtsShow('plain'); }
gtsPlain();

// Every step of a foreach names that foreach.
function gtsGen() { gtsShow('first'); yield 1; gtsShow('second'); yield 2; }
foreach (gtsGen() as $v) {}

// A delegate names the `yield from`, and the delegator names its own driver.
function gtsInner() { gtsShow('inner'); yield 3; }
function gtsOuter() { yield from gtsInner(); }
foreach (gtsOuter() as $v) {}

// Two frames down, through a function that drives the loop.
function gtsHost() { foreach (gtsGen() as $v) { break; } }
gtsHost();

// A method generator, and a generator iterated inside another generator.
class GtsC { public function m() { gtsShow('method'); yield 4; } }
foreach ((new GtsC())->m() as $v) {}
function gtsWrap() { foreach (gtsInner() as $v) { yield $v; } }
foreach (gtsWrap() as $v) {}

/* The RECEIVER of a generator method is on its frame too, not only in its
 * variable table: debug_backtrace() reports a frame's `object`, and a library
 * that finds the culprit by scanning the backtrace for one (twig's error
 * reporter looks for `$trace['object'] instanceof Template`) sees nothing
 * without it. */
function gtsObjects()
{
    $out = '';
    foreach (debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS | DEBUG_BACKTRACE_PROVIDE_OBJECT) as $f) {
        if (($f['file'] ?? null) !== __FILE__ && isset($f['file'])) { continue; }
        /* php gives the INTERNAL Generator method its own frame and PHL has no
         * frames for internal functions at all — a gap of its own,
         * not this one, so leave those out. */
        if (($f['class'] ?? null) === 'Generator') { continue; }
        $out .= ' ' . ($f['class'] ?? '-') . '/' . $f['function']
             . '/' . (isset($f['object']) ? get_class($f['object']) : 'none');
    }
    echo 'objects', $out, "\n";
}
class GtsRecv
{
    public function gen() { gtsObjects(); yield 1; }
    public function plain() { gtsObjects(); }
    public static function stat() { gtsObjects(); }
}
$gtsR = new GtsRecv();
$gtsR->plain();
GtsRecv::stat();
foreach ($gtsR->gen() as $v) {}
$gtsGenObj = $gtsR->gen();
$gtsGenObj->current();
echo 'reflgen ', get_class((new ReflectionGenerator($gtsGenObj))->getThis()), "\n";
?>
--EXPECT--
plain 23/gtsShow 24/gtsPlain
first 27/gtsShow 28/gtsGen
second 27/gtsShow 28/gtsGen
inner 31/gtsShow 32/gtsInner 33/gtsOuter
first 27/gtsShow 36/gtsGen 37/gtsHost
method 40/gtsShow 41/m
inner 31/gtsShow 42/gtsInner 43/gtsWrap
objects -/gtsObjects/none GtsRecv/plain/GtsRecv
objects -/gtsObjects/none GtsRecv/stat/none
objects -/gtsObjects/none GtsRecv/gen/GtsRecv
objects -/gtsObjects/none GtsRecv/gen/GtsRecv
reflgen GtsRecv
