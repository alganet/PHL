--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
after an outer try catches a throw raised several calls deep, the continuation runs in the catching frame's own scope (outer locals stay live)
--FILE--
<?php
/* Regression: VmRecordedResume used to leave pVm->pFrame pointing at a dead
 * callee frame when a DEEP throw was caught by an OUTER try, so code after the
 * try resolved the catching function's own locals against the wrong scope and
 * read them as fresh nulls. This is the PHPUnit "member function ... on null"
 * blocker distilled: $emitter (a pre-try local) went null after runBare caught
 * a constraint failure raised deep inside runTest. */
class NwpResumeObj {
    public function ping(): string { return "pong"; }
}
function nwpResumeDeep(): void {
    throw new RuntimeException("deep");
}
function nwpResumeMid(): void {
    /* an inner try that does NOT match the thrown type: it pushes its own
     * exception frame and re-propagates, reproducing the intermediate frames
     * the resume path has to tear down. */
    try {
        nwpResumeDeep();
    } catch (LogicException $bad) {
        echo "unexpected-inner-catch";
    }
}
function nwpResumeOuter(): string {
    $emitter = new NwpResumeObj();   // pre-try outer local (the $emitter analogue)
    $marker  = "kept";
    try {
        nwpResumeMid();              // throws two calls deep; caught right here
    } catch (Throwable $e) {
        $caught = $e->getMessage();  // written in the catch, must persist
    }
    /* All three must resolve in THIS frame after the catch. */
    return $emitter->ping() . "/" . $marker . "/" . $caught;
}
echo nwpResumeOuter(), "\n";

/* And the non-exception continuation guard must see the right scope too: with a
 * successful (non-throwing) call the same tail runs, and isset() reflects it. */
function nwpResumeNoThrow(): void {}
function nwpResumeGuard(): string {
    $emitter = new NwpResumeObj();
    try {
        nwpResumeNoThrow();
    } catch (Throwable $e) {
        return "threw";
    }
    return !isset($e) ? ("clean/" . $emitter->ping()) : "stale-e";
}
echo nwpResumeGuard(), "\n";
?>
--EXPECT--
pong/kept/deep
clean/pong
--CLEAN--
<?php
