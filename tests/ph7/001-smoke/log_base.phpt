--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
log() honours its base, and takes 10 and 2 from libm rather than by division
--DESCRIPTION--
log()'s second argument was read with an integer conversion and compared against
10, so every other base -- 2 included -- silently answered the natural logarithm:
log(8, 2) was 2.0794415416798357 where php answers 3.0. A wrong number with no
diagnostic on it. php takes base 10 and base 2 through log10()/log2() before any
other screen, because the division does not reproduce them (log(1e300)/log(10) is
299.99999999999994 where log10(1e300) is 300); then a base at or below zero is a
ValueError, base 1 is NAN, and every other base divides.
--FILE--
<?php
function logShow($logNum, $logBase = 'default') {
    $logLabel = 'log(' . var_export($logNum, true)
        . ($logBase === 'default' ? '' : ', ' . var_export($logBase, true)) . ')';
    try {
        $logOut = var_export($logBase === 'default' ? log($logNum) : log($logNum, $logBase), true);
    } catch (Throwable $logE) {
        $logOut = get_class($logE) . ': ' . $logE->getMessage();
    }
    echo $logLabel, ' => ', $logOut, "\n";
}

/* The base is honoured at all. */
logShow(8, 2);
logShow(8, 3);
logShow(8, 0.5);
logShow(100, 10);
logShow(1024, 2);
logShow(243, 3);

/* No base is the natural logarithm, and so is the default base M_E. */
logShow(8);
logShow(8, M_E);
var_dump(log(8, M_E) === log(8));

/* 10 and 2 come from libm, not from a division: the division loses the exact
   answer at the top of the range. */
logShow(1e300, 10);
var_dump(log(1e300) / log(10));
logShow(1e300, 2);

/* A base at or below zero is a ValueError -- and it is raised after the 10 and 2
   screens, so it can never fire for those. */
logShow(8, 0);
logShow(8, -1);
logShow(8, -2.5);

/* Base 1 is NAN, not a division by zero. */
logShow(8, 1);
logShow(1, 1);
logShow(0, 1);

/* Non-finite bases divide like any other. */
logShow(8, INF);
logShow(0, INF);
logShow(INF, INF);
logShow(8, NAN);

/* The first argument's own edges are unchanged by any base. */
foreach ([0, -1, INF, -INF, NAN] as $logEdge) {
    logShow($logEdge);
    logShow($logEdge, 2);
    logShow($logEdge, 3);
}

/* A numeric string base is coerced, like any float parameter. */
logShow(8, '2');
logShow(8, '10');
logShow(8, true);

/* Argument shapes. */
try { log(8, []); } catch (Throwable $logE) { echo get_class($logE), ': ', $logE->getMessage(), "\n"; }
try { log('x'); } catch (Throwable $logE) { echo get_class($logE), ': ', $logE->getMessage(), "\n"; }
try { log(); } catch (Throwable $logE) { echo get_class($logE), ': ', $logE->getMessage(), "\n"; }
try { log(1, 2, 3); } catch (Throwable $logE) { echo get_class($logE), ': ', $logE->getMessage(), "\n"; }
--EXPECT--
log(8, 2) => 3.0
log(8, 3) => 1.892789260714372
log(8, 0.5) => -3.0
log(100, 10) => 2.0
log(1024, 2) => 10.0
log(243, 3) => 4.999999999999999
log(8) => 2.0794415416798357
log(8, 2.718281828459045) => 2.0794415416798357
bool(true)
log(1.0E+300, 10) => 300.0
float(299.99999999999994)
log(1.0E+300, 2) => 996.5784284662087
log(8, 0) => ValueError: log(): Argument #2 ($base) must be greater than 0
log(8, -1) => ValueError: log(): Argument #2 ($base) must be greater than 0
log(8, -2.5) => ValueError: log(): Argument #2 ($base) must be greater than 0
log(8, 1) => NAN
log(1, 1) => NAN
log(0, 1) => NAN
log(8, INF) => 0.0
log(0, INF) => NAN
log(INF, INF) => NAN
log(8, NAN) => NAN
log(0) => -INF
log(0, 2) => -INF
log(0, 3) => -INF
log(-1) => NAN
log(-1, 2) => NAN
log(-1, 3) => NAN
log(INF) => INF
log(INF, 2) => INF
log(INF, 3) => INF
log(-INF) => NAN
log(-INF, 2) => NAN
log(-INF, 3) => NAN
log(NAN) => NAN
log(NAN, 2) => NAN
log(NAN, 3) => NAN
log(8, '2') => 3.0
log(8, '10') => 0.9030899869919435
log(8, true) => NAN
TypeError: log(): Argument #2 ($base) must be of type float, array given
TypeError: log(): Argument #1 ($num) must be of type float, string given
ArgumentCountError: log() expects at least 1 argument, 0 given
ArgumentCountError: log() expects at most 2 arguments, 3 given
