--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A value no int can hold is refused by a typed int property, not stored wrapped
--FILE--
<?php
/* php refuses a value outside the int64 range for an `int` slot outright -- this
 * is its TypeError, not one of its deprecations -- and a typed property store
 * had none of that rule, so the slot took whatever the truncation produced:
 * PHP_INT_MIN for a float too large, PHP_INT_MAX for a numeric string too long,
 * both silently and neither anywhere near the value assigned. */
class TPOutOfRange {
    public int $i = 0;
    public static int $s = 0;
    public ?int $n = null;
}
function tp_store(string $label, callable $fn): void {
    try {
        var_dump($fn());
    } catch (\Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
$o = new TPOutOfRange();
echo "## a float outside the range\n";
tp_store('1e20', function () use ($o) { $o->i = 1e20; return $o->i; });
tp_store('-1e20', function () use ($o) { $o->i = -1e20; return $o->i; });
tp_store('INF', function () use ($o) { $o->i = INF; return $o->i; });
tp_store('NAN', function () use ($o) { $o->i = NAN; return $o->i; });
tp_store('static', function () { TPOutOfRange::$s = 1e20; return TPOutOfRange::$s; });
tp_store('nullable', function () use ($o) { $o->n = 1e20; return $o->n; });

echo "## a numeric string longer than an int\n";
tp_store('digits', function () use ($o) { $o->i = "99999999999999999999"; return $o->i; });
tp_store('negative digits', function () use ($o) { $o->i = "-99999999999999999999"; return $o->i; });
tp_store('exponent', function () use ($o) { $o->i = "1e20"; return $o->i; });

echo "## what a whole value still does\n";
tp_store('5.0', function () use ($o) { $o->i = 5.0; return $o->i; });
tp_store('"2e1"', function () use ($o) { $o->i = "2e1"; return $o->i; });
tp_store('"2.0"', function () use ($o) { $o->i = "2.0"; return $o->i; });
tp_store('"42"', function () use ($o) { $o->i = "42"; return $o->i; });
tp_store('PHP_INT_MAX', function () use ($o) { $o->i = PHP_INT_MAX; return $o->i; });
tp_store('true', function () use ($o) { $o->i = true; return $o->i; });
?>
--EXPECT--
## a float outside the range
TypeError: Cannot assign float to property TPOutOfRange::$i of type int
TypeError: Cannot assign float to property TPOutOfRange::$i of type int
TypeError: Cannot assign float to property TPOutOfRange::$i of type int
TypeError: Cannot assign float to property TPOutOfRange::$i of type int
TypeError: Cannot assign float to property TPOutOfRange::$s of type int
TypeError: Cannot assign float to property TPOutOfRange::$n of type ?int
## a numeric string longer than an int
TypeError: Cannot assign string to property TPOutOfRange::$i of type int
TypeError: Cannot assign string to property TPOutOfRange::$i of type int
TypeError: Cannot assign string to property TPOutOfRange::$i of type int
## what a whole value still does
int(5)
int(20)
int(2)
int(42)
int(9223372036854775807)
int(1)
