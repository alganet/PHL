--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwing autoloader propagates from a callable dispatch, with no extra "not found"
--DESCRIPTION--
Resolving the class half of a callable RUNS the autoloader, so the autoloader's
own exception is what php propagates — it never also reports the class missing.
PHL's dispatch checked "did the class resolve?" and, on the 0 that a throwing
autoloader leaves behind, threw its own `Class "X" not found` Error ON TOP: the
real exception was caught by the surrounding try, and then the second Error —
which belongs to nobody — came back uncaught and killed the script. The check
now notices the boundary rail moved (a parked status, or a resume frame from an
in-place catch) and lands that throw instead.

The call_user_func side is NOT covered here: php's own ZPP callable check DROPS the
autoloader's exception and reports only its TypeError, and PHL raises both — a
separate divergence, tracked with the callback-argument reason work.

Its own process: the autoloader registered here is global state.
--FILE--
<?php
spl_autoload_register(function ($name) {
    if (str_starts_with($name, 'CdatBoom')) {
        throw new RuntimeException("autoload failed for $name");
    }
    /* Anything else: stay quiet, like a real autoloader that does not own the name. */
});

function cdatRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

cdatRun('array form', function () { $cb = ['CdatBoom1', 'm']; return $cb(); });
cdatRun('string form', function () { $cb = 'CdatBoom2::m'; return $cb(); });
cdatRun('is_callable', function () { return is_callable(['CdatBoom4', 'm']); });

/* A name the autoloader does not own still reports php's missing-class Error. */
cdatRun('quiet autoloader', function () { $cb = 'CdatMissing::m'; return $cb(); });
cdatRun('quiet autoloader array', function () { $cb = ['CdatMissing', 'm']; return $cb(); });
echo "end\n";
?>
--EXPECT--
array form => RuntimeException: autoload failed for CdatBoom1
string form => RuntimeException: autoload failed for CdatBoom2
is_callable => RuntimeException: autoload failed for CdatBoom4
quiet autoloader => Error: Class "CdatMissing" not found
quiet autoloader array => Error: Class "CdatMissing" not found
end
