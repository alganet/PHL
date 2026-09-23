--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types refuses a coerced argument to a builtin and to a native method too
--FILE--
<?php
declare(strict_types=1);
function stbTry(callable $f) {
    try { $r = $f(); echo "ok "; var_dump($r); }
    catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
class StbStringable { public function __toString(): string { return "str"; } }

// no scalar coercion at an internal call either
stbTry(fn() => trim(5));
stbTry(fn() => trim(5.5));
stbTry(fn() => trim(true));
stbTry(fn() => ucfirst(5));
stbTry(fn() => str_repeat("a", "2"));
stbTry(fn() => str_repeat("a", 2.0));
stbTry(fn() => str_repeat("a", true));
stbTry(fn() => sqrt("4"));
stbTry(fn() => sqrt(true));
stbTry(fn() => intdiv(7.0, 2));
$one = 1;
stbTry(fn() => in_array(1, [1], $one));
stbTry(fn() => strlen(new StbStringable));

// the one widening strict mode keeps, and the types that already matched
stbTry(fn() => sqrt(4));
stbTry(fn() => trim("  x  "));
stbTry(fn() => str_repeat("a", 2));
stbTry(fn() => in_array(1, [1], true));
stbTry(fn() => date('Y', 100));
stbTry(fn() => date('Y', "100"));

// a `callable` parameter still takes a function-name string
stbTry(fn() => array_map('strtoupper', ['a'])[0]);

// native methods take the same rule
$it = new ArrayIterator([1, 2, 3]);
stbTry(function () use ($it) { $it->seek("1"); return $it->current(); });
stbTry(function () use ($it) { $it->seek(1); return $it->current(); });

// an array, a null and a resource are refused in both modes, unchanged
stbTry(fn() => strlen([1]));
stbTry(fn() => strlen(null));
?>
--EXPECT--
trim(): Argument #1 ($string) must be of type string, int given
trim(): Argument #1 ($string) must be of type string, float given
trim(): Argument #1 ($string) must be of type string, true given
ucfirst(): Argument #1 ($string) must be of type string, int given
str_repeat(): Argument #2 ($times) must be of type int, string given
str_repeat(): Argument #2 ($times) must be of type int, float given
str_repeat(): Argument #2 ($times) must be of type int, true given
sqrt(): Argument #1 ($num) must be of type float, string given
sqrt(): Argument #1 ($num) must be of type float, true given
intdiv(): Argument #1 ($num1) must be of type int, float given
in_array(): Argument #3 ($strict) must be of type bool, int given
strlen(): Argument #1 ($string) must be of type string, StbStringable given
ok float(2)
ok string(1) "x"
ok string(2) "aa"
ok bool(true)
ok string(4) "1970"
date(): Argument #2 ($timestamp) must be of type ?int, string given
ok string(1) "A"
ArrayIterator::seek(): Argument #1 ($offset) must be of type int, string given
ok int(2)
strlen(): Argument #1 ($string) must be of type string, array given
strlen(): Argument #1 ($string) must be of type string, null given
