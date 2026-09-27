--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every generator accessor initializes, and a run generator is not rewindable
--FILE--
<?php
/* php's zend_generator_ensure_initialized runs a never-executed body to its
 * first yield before ANY accessor answers, and the priming run is not the
 * advance. `valid()` never did it here, so `while ($g->valid())` was skipped
 * entirely; `next()`/`send()` called the priming run the advance, so the first
 * element came out twice and the first `send()` value was dropped. */
function genContractBody()
{
    $x = yield 1;
    echo "got:", var_export($x, true), "\n";
    yield 2;
    return "R";
}
function genContractEmpty()
{
    if (false) {
        yield;
    }
    return "E";
}
function genContractRun($label, callable $fn)
{
    echo "-- $label\n";
    try {
        $fn();
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

genContractRun('valid primes',   function () { $g = genContractBody(); var_dump($g->valid()); });
genContractRun('send primes then resumes', function () { $g = genContractBody(); var_dump($g->send('S')); });
genContractRun('next primes then advances', function () { $g = genContractBody(); $g->next(); var_dump($g->current()); });
genContractRun('getReturn primes and refuses', function () { $g = genContractBody(); var_dump($g->getReturn()); });
genContractRun('while(valid)',   function () {
    $g = genContractBody();
    while ($g->valid()) { echo $g->key(), '=>', $g->current(), "\n"; $g->next(); }
    var_dump($g->getReturn());
});

/* rewind() is only allowed to mean "initialize" */
genContractRun('rewind fresh',        function () { $g = genContractBody(); $g->rewind(); $g->rewind(); var_dump($g->current()); });
genContractRun('rewind after next',   function () { $g = genContractBody(); $g->next(); $g->rewind(); });
genContractRun('rewind empty body',   function () { $g = genContractEmpty(); $g->rewind(); var_dump($g->getReturn()); });
genContractRun('foreach half-eaten',  function () { $g = genContractBody(); $g->next(); foreach ($g as $v) { var_dump($v); } });
genContractRun('foreach twice',       function () { $g = genContractBody(); foreach ($g as $v) {} foreach ($g as $v) {} });
genContractRun('iterator_to_array twice', function () { $g = genContractBody(); iterator_to_array($g); var_dump(iterator_to_array($g)); });
genContractRun('spread a run generator', function () { $g = genContractBody(); foreach ($g as $v) {} var_dump([...$g]); });

/* `yield from` INITIALIZES its delegate rather than rewinding it, so a
 * half-consumed generator continues; a run one is php's own Error. */
genContractRun('yield from half-eaten', function () {
    $g = genContractBody();
    $g->current();
    $o = (function () use ($g) { yield from $g; return 'D'; })();
    foreach ($o as $v) { var_dump($v); }
    var_dump($o->getReturn());
});
genContractRun('yield from a run generator', function () {
    $g = genContractBody();
    foreach ($g as $v) {}
    $o = (function () use ($g) { yield from $g; })();
    foreach ($o as $v) {}
});
?>
--EXPECT--
-- valid primes
bool(true)
-- send primes then resumes
got:'S'
int(2)
-- next primes then advances
got:NULL
int(2)
-- getReturn primes and refuses
Exception: Cannot get return value of a generator that hasn't returned
-- while(valid)
0=>1
got:NULL
1=>2
string(1) "R"
-- rewind fresh
int(1)
-- rewind after next
got:NULL
Exception: Cannot rewind a generator that was already run
-- rewind empty body
string(1) "E"
-- foreach half-eaten
got:NULL
Exception: Cannot rewind a generator that was already run
-- foreach twice
got:NULL
Exception: Cannot traverse an already closed generator
-- iterator_to_array twice
got:NULL
Exception: Cannot traverse an already closed generator
-- spread a run generator
got:NULL
Exception: Cannot traverse an already closed generator
-- yield from half-eaten
int(1)
got:NULL
int(2)
string(1) "D"
-- yield from a run generator
got:NULL
Error: Generator passed to yield from was aborted without proper return and is unable to continue
--CLEAN--
<?php
