--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The incompatible-declaration fatal prints php's DEFAULT for each parameter
--DESCRIPTION--
php renders the declaration from what its compiler FOLDED, with two exceptions it
leaves unfolded and prints as source: a lone constant reference keeps its NAME and
a class constant keeps `Class::NAME`. `X::class` is not one of those -- it folds to
the class-name string. A string is single-quoted, printed raw and cut to ten bytes
with `...`; an array shows only whether it is empty; what the folder gave up on
(here `PHP_EOL . 'x'`) is `<expression>`.
--FILE--
<?php
class DrdA {
    const K = 7;
    public function f(
        $radix = 0777,
        $product = 2 * 1024,
        $glued = 'a' . 'b',
        $long = 'abcdefghijklmnop',
        $named = PHP_INT_MAX,
        $classK = self::K,
        $cls = DrdA::class,
        $empty = [],
        $full = [1, 2],
        $gaveUp = PHP_EOL . 'x'
    ) {}
}
class DrdB extends DrdA {
    public function f() {}
}
echo "unreached\n";
?>
--EXPECTF--
%ADeclaration of DrdB::f() must be compatible with DrdA::f($radix = 511, $product = 2048, $glued = 'ab', $long = 'abcdefghij...', $named = PHP_INT_MAX, $classK = self::K, $cls = 'DrdA', $empty = [], $full = [...], $gaveUp = <expression>)%A
--CLEAN--
<?php
