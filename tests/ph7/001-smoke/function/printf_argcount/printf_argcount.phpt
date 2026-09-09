--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf family throws when given fewer value arguments than the format needs (PHP 8)
--FILE--
<?php
function fmtArgCountCases(): array {
    $catch = function (callable $fn): string {
        try { $fn(); return "no-throw"; } catch (\Throwable $e) { return get_class($e) . ": " . $e->getMessage(); }
    };
    $mem = fopen("php://memory", "r+");
    $out = [];
    $out[] = $catch(fn() => sprintf("%d"));
    $out[] = $catch(fn() => sprintf("%d-%d", 5));
    $out[] = $catch(fn() => sprintf('%1$s %2$s', "a"));
    $out[] = $catch(fn() => sprintf('%2$s', "a"));
    $out[] = $catch(fn() => printf("%s%s", "a"));
    $out[] = $catch(fn() => vsprintf("%d%d", [5]));
    $out[] = $catch(fn() => vsprintf("%d", []));
    $out[] = $catch(fn() => fprintf($mem, "%d-%d", 5));
    $out[] = $catch(fn() => vfprintf($mem, "%d %d", [1]));
    // valid cases must NOT throw
    $out[] = sprintf("%d-%d", 5, 6);
    $out[] = sprintf('%1$s %1$s', "a");
    $out[] = sprintf("%% only");
    $out[] = vsprintf("%s", ["a", "b", "c"]);
    fclose($mem);
    return $out;
}
echo implode("\n", fmtArgCountCases()), "\n";
--EXPECT--
ArgumentCountError: 2 arguments are required, 1 given
ArgumentCountError: 3 arguments are required, 2 given
ArgumentCountError: 3 arguments are required, 2 given
ArgumentCountError: 3 arguments are required, 2 given
ArgumentCountError: 3 arguments are required, 2 given
ValueError: The arguments array must contain 2 items, 1 given
ValueError: The arguments array must contain 1 items, 0 given
ArgumentCountError: 4 arguments are required, 3 given
ValueError: The arguments array must contain 2 items, 1 given
5-6
a a
% only
a
--CLEAN--
<?php
