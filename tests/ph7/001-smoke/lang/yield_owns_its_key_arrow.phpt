--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array entry that starts with yield owns the first "=>" after it
--FILE--
<?php
/* php's grammar gives `yield expr => expr` to the yield, so `[yield 1 => 2]` is
 * ONE element -- the yield's own result, keyed by nothing -- and the generator
 * yields key 1 value 2. Reading that arrow as the entry separator instead built
 * `[(yield 1) => 2]`: a different array AND a different yielded pair, in
 * silence. A SECOND top-level arrow is the entry separator again. */
function yieldArrowDrive($label, callable $mk)
{
    echo "-- $label\n";
    $g = $mk();
    $n = 0;
    while ($g->valid() && $n < 6) {
        echo '  y ', json_encode($g->key()), '=>', json_encode($g->current()), "\n";
        $g->send('S');
        $n++;
    }
}

yieldArrowDrive('bare pair',        fn() => (function () { $a = [yield 1 => 2]; echo '  arr ', json_encode($a), "\n"; })());
yieldArrowDrive('two arrows',       fn() => (function () { $a = [yield 1 => 2 => 3]; echo '  arr ', json_encode($a), "\n"; })());
yieldArrowDrive('parenthesised',    fn() => (function () { $a = [(yield 1) => 2]; echo '  arr ', json_encode($a), "\n"; })());
yieldArrowDrive('as a value',       fn() => (function () { $a = ['k' => yield 1 => 2]; echo '  arr ', json_encode($a), "\n"; })());
yieldArrowDrive('after an element', fn() => (function () { $a = [1, yield 2 => 3]; echo '  arr ', json_encode($a), "\n"; })());
yieldArrowDrive('before an element', fn() => (function () { $a = [yield 'k' => 'v', 9]; echo '  arr ', json_encode($a), "\n"; })());

/* the two shapes whose "=>" is never a key separator: an arrow function's body
 * and a match arm's -- both were read as one, the first a parse error */
yieldArrowDrive('yield an arrow fn', fn() => (function () { $a = [2 => [1, 2], yield fn($x) => $x]; echo '  arr ', json_encode(array_keys($a)), "\n"; })());
yieldArrowDrive('yield a match',     fn() => (function () { $a = [yield match (1) { 1 => 7, default => 8 }]; echo '  arr ', json_encode($a), "\n"; })());

/* yield from takes an iterable and never a pair, so its entry keeps the
 * ordinary rule */
yieldArrowDrive('yield from', fn() => (function () { $a = [yield from [1, 2]]; echo '  arr ', json_encode($a), "\n"; })());
?>
--EXPECT--
-- bare pair
  y 1=>2
  arr ["S"]
-- two arrows
  y 1=>2
  arr {"S":3}
-- parenthesised
  y 0=>1
  arr {"S":2}
-- as a value
  y 1=>2
  arr {"k":"S"}
-- after an element
  y 2=>3
  arr [1,"S"]
-- before an element
  y "k"=>"v"
  arr ["S",9]
-- yield an arrow fn
  y 0=>{}
  arr [2,3]
-- yield a match
  y 0=>7
  arr ["S"]
-- yield from
  y 0=>1
  y 1=>2
  arr [null]
--CLEAN--
<?php
