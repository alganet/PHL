--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mt_rand()/rand() range + arg-count errors and signed ranges
--FILE--
<?php
function rng_probe($fn): string {
    try {
        $fn();
        return "no-throw";
    } catch (\Throwable $e) {
        return get_class($e) . ": " . $e->getMessage();
    }
}
function in_range(int $r, int $lo, int $hi): bool {
    return $r >= $lo && $r <= $hi;
}
// mt_rand() is strict: a reversed range is a catchable ValueError
echo rng_probe(fn() => mt_rand(5, 1)), "\n";
// rand() keeps php's backward-compat swap for a reversed range
echo (in_range(rand(5, 1), 1, 5) ? "swap-ok" : "swap-bad"), "\n";
// wrong argument count -> ArgumentCountError (exactly 0 or 2 args)
echo rng_probe(fn() => rand(5)), "\n";
echo rng_probe(fn() => mt_rand(5)), "\n";
echo rng_probe(fn() => rand(1, 2, 3)), "\n";
echo rng_probe(fn() => mt_rand(1, 2, 3)), "\n";
// signed ranges: negatives and equal bounds land in [min,max]
$ok = true;
for ($i = 0; $i < 200; $i++) {
    $ok = $ok
        && in_range(rand(-10, -1), -10, -1)
        && in_range(mt_rand(-10, -1), -10, -1)
        && in_range(rand(-5, 5), -5, 5)
        && in_range(mt_rand(1, 6), 1, 6);
}
echo ($ok ? "signed-ok" : "signed-bad"), "\n";
// equal bounds return exactly that value
echo (rand(7, 7) === 7 && mt_rand(0, 0) === 0 ? "equal-ok" : "equal-bad"), "\n";
// no-arg form still returns a non-negative int
echo (is_int(rand()) && rand() >= 0 && is_int(mt_rand()) ? "noarg-ok" : "noarg-bad"), "\n";
?>
--EXPECT--
ValueError: mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)
swap-ok
ArgumentCountError: rand() expects exactly 2 arguments, 1 given
ArgumentCountError: mt_rand() expects exactly 2 arguments, 1 given
ArgumentCountError: rand() expects exactly 2 arguments, 3 given
ArgumentCountError: mt_rand() expects exactly 2 arguments, 3 given
signed-ok
equal-ok
noarg-ok
--CLEAN--
<?php
