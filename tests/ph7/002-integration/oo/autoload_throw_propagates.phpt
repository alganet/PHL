--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwing autoloader is the only error: no "Class X not found" piled on top
--DESCRIPTION--
Every site that resolves a class NAME can run the autoloader, and php reports
the autoloader's exception and nothing else. `new`, the static-member opcode
(method call, class constant, static property) and the callable dispatch all
looked at the 0 the failed lookup left and threw their own `Class "X" not
found` Error ON TOP: the real exception was caught by the surrounding try, and
then the second Error — which belongs to nobody — came back UNCAUGHT and killed
the script. Each of those sites now checks whether the boundary rail moved
during the lookup (a parked status, or a resume frame from an in-place catch)
and lands that throw instead.

Its own process: the autoloader is global state.
--FILE--
<?php
spl_autoload_register(function ($name) {
    if (str_starts_with($name, 'AtpBoom')) {
        throw new RuntimeException("autoload failed for $name");
    }
});

function atpRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

atpRun('new', fn() => new AtpBoom1());
atpRun('new with args', fn() => new AtpBoom2(1, 2));
atpRun('static method', fn() => AtpBoom3::m());
atpRun('class constant', fn() => AtpBoom4::K);
atpRun('static property', fn() => AtpBoom5::$p);
atpRun('array callable', function () { $cb = ['AtpBoom6', 'm']; return $cb(); });
atpRun('string callable', function () { $cb = 'AtpBoom7::m'; return $cb(); });

/* A name the autoloader does not own still reports php's missing-class Error. */
atpRun('quiet new', fn() => new AtpMissing());
atpRun('quiet static call', fn() => AtpMissing::m());

/* Execution continues normally afterwards. */
echo 'still here', "\n";
echo "end\n";
?>
--EXPECT--
new => RuntimeException: autoload failed for AtpBoom1
new with args => RuntimeException: autoload failed for AtpBoom2
static method => RuntimeException: autoload failed for AtpBoom3
class constant => RuntimeException: autoload failed for AtpBoom4
static property => RuntimeException: autoload failed for AtpBoom5
array callable => RuntimeException: autoload failed for AtpBoom6
string callable => RuntimeException: autoload failed for AtpBoom7
quiet new => Error: Class "AtpMissing" not found
quiet static call => Error: Class "AtpMissing" not found
still here
end
