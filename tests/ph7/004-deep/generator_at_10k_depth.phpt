--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator created at 10k call depth, and resumed from a fresh 10k-deep chain (BYTECODE.md stage 3)
--DESCRIPTION--
The ctx start/resume native re-entries must be indifferent to PHP call depth:
the generator body runs as the bottom record of its own dispatch invocation
while the surrounding 10k frames are heap records.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only deep-recursion probe: depth exceeds the php oracle stack / xdebug nesting limit'; ?>
--FILE--
<?php
function counter(int $from): Generator {
    while (true) {
        yield $from++;
    }
}
function makeAtDepth(int $n): Generator {
    if ($n === 0) {
        $g = counter(100);
        $g->current(); // prime at depth
        return $g;
    }
    return makeAtDepth($n - 1);
}
function resumeAtDepth(int $n, Generator $g): int {
    if ($n === 0) {
        $g->next();
        return $g->current();
    }
    return resumeAtDepth($n - 1, $g);
}
$g = makeAtDepth(10000);
echo "primed=", $g->current(), "\n";
echo "resumed=", resumeAtDepth(10000, $g), "\n";
echo "again=", resumeAtDepth(10000, $g), "\n";
?>
--EXPECT--
primed=100
resumed=101
again=102
--CLEAN--
<?php
unset($g);
