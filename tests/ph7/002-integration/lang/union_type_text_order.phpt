--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the canonical UNION text: classes first, then php's built-in order, iterable expanded
--FILE--
<?php
class Oth {}
class A1 {}
class B1 {}
interface I1 {}
interface I2 {}

// One function per shape; the declared order is deliberately NOT php's canonical
// order, so the text can only come out right by being rebuilt.
function u01(int|A1 $x) {}
function u02(A1|int $x) {}
function u03(string|int|A1 $x) {}
function u04(A1|B1|int $x) {}
function u05(int|float|string $x) {}
function u06(int|false $x) {}
function u07(false|int $x) {}
function u08(false|A1 $x) {}
function u09(array|false $x) {}
function u10(int|true $x) {}
function u11(callable|int $x) {}
function u12(int|callable $x) {}
function u13(callable|A1 $x) {}
function u14(callable|object $x) {}
function u15(iterable|int $x) {}
function u16(int|iterable $x) {}
function u17(iterable $x) {}
function u18(A1|iterable $x) {}
function u19(callable|iterable $x) {}
function u20(iterable|callable|object $x) {}
function u21(float|false|int $x) {}
function u22(bool|A1|array $x) {}
function u23(int|null|A1 $x) {}
function u24(?iterable $x) {}
function u25((I1&I2)|false $x) {}
function u26((I1&I2)|iterable $x) {}

// No single value is rejected by every shape here (an int satisfies the `int`
// members, an object satisfies the `object` ones), so try both and report the
// first rejection.
$bad = [5, new Oth];

// The TypeError message and ReflectionType::__toString() print the SAME
// canonical text, so assert both off one declaration. The two DIFFERS rows are
// php's own: a STANDALONE `iterable` keeps its name in reflection while the
// TypeError expands it.
for ($i = 1; $i <= 26; $i++) {
    $f = sprintf('u%02d', $i);
    $r = new ReflectionFunction($f);
    $viaReflection = (string) $r->getParameters()[0]->getType();
    $viaError = '(never rejected)';
    foreach ($bad as $v) {
        if ($viaError !== '(never rejected)') { continue; }
        try { $f($v); }
        catch (TypeError $e) {
            // "u01(): Argument #1 ($x) must be of type <TEXT>, <kind> given"
            $m = substr($e->getMessage(), strlen("$f(): Argument #1 (\$x) must be of type "));
            $viaError = substr($m, 0, strpos($m, ', '));
        }
    }
    echo $f, ' ', $viaReflection, ' -> ',
        ($viaError === $viaReflection ? 'same' : "DIFFERS: $viaError"), "\n";
}
?>
--EXPECT--
u01 A1|int -> same
u02 A1|int -> same
u03 A1|string|int -> same
u04 A1|B1|int -> same
u05 string|int|float -> same
u06 int|false -> same
u07 int|false -> same
u08 A1|false -> same
u09 array|false -> same
u10 int|true -> same
u11 callable|int -> same
u12 callable|int -> same
u13 A1|callable -> same
u14 callable|object -> same
u15 Traversable|array|int -> same
u16 Traversable|array|int -> same
u17 iterable -> DIFFERS: Traversable|array
u18 A1|Traversable|array -> same
u19 Traversable|callable|array -> same
u20 Traversable|callable|object|array -> same
u21 int|float|false -> same
u22 A1|array|bool -> same
u23 A1|int|null -> same
u24 ?iterable -> DIFFERS: Traversable|array|null
u25 (I1&I2)|false -> same
u26 (I1&I2)|Traversable|array -> same
--CLEAN--
<?php
