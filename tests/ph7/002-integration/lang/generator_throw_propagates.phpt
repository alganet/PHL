--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() propagates the actual exception object (class + message) to the caller
--FILE--
<?php
function gen() { yield 1; yield 2; }
$g = gen();
echo $g->current(), "\n";
try {
    $g->throw(new RuntimeException("boom"));
} catch (RuntimeException $e) {
    echo "caught ", get_class($e), ": ", $e->getMessage(), "\n";
}
// Throwing into an already-finished generator also propagates the object.
function done() { if (false) { yield; } }
$d = done();
$d->current();
try {
    $d->throw(new LogicException("after-end"));
} catch (LogicException $e) {
    echo "caught ", get_class($e), ": ", $e->getMessage(), "\n";
}
// Throwing into a *completed* generator must not destroy its return value.
function ret() { yield 1; return 42; }
$r = ret();
$r->current();
$r->next();                       // runs to completion (return 42)
try {
    $r->throw(new RuntimeException("late"));
} catch (RuntimeException $e) {
    echo "caught ", $e->getMessage(), "\n";
}
echo "getReturn=", $r->getReturn(), "\n";   // still 42, state preserved
// Throwing into a generator that is currently executing is an Error.
$run = (function () {
    $me = yield 1;
    try {
        $me->throw(new RuntimeException("boom"));
    } catch (\Error $e) {
        echo "running: ", $e->getMessage(), "\n";
    }
    yield 2;
})();
$run->current();
$run->send($run);
echo "end\n";
?>
--EXPECT--
1
caught RuntimeException: boom
caught LogicException: after-end
caught late
getReturn=42
running: Cannot resume an already running generator
end
--CLEAN--
<?php
