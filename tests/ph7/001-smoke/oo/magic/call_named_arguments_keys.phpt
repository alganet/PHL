--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__call/__callStatic receive named arguments keyed by name in $args
--DESCRIPTION--
php packs the catch-all's `$args` with the NAMES the call was made with:
`$o->m(a: 1)` and `$o->m(...['a' => 1])` both arrive as `['a' => 1]`. Every
argument went in at an auto index, so a handler reading `$args['a']` found
nothing and one reading `$args[0]` was handed a value php would never have put
there. Both packing sites had the bug and there were two of them — the
`$o->m()` syntax and every callable spelling — so this also pins that they agree.
A string-keyed unpack is the case a compile-time name map cannot cover: the
EFFECTIVE map is what carries it.
--FILE--
<?php
class CnakMagic {
    public function __call($name, $args) { return $name . '=' . json_encode($args); }
    public static function __callStatic($name, $args) { return 'S:' . $name . '=' . json_encode($args); }
}
$o = new CnakMagic;

/* The `$o->m()` / `C::m()` syntax. */
echo $o->a(1, b: 2), "\n";
echo $o->b(x: 1, y: 2, z: 3), "\n";
echo CnakMagic::c(1, k: 'v'), "\n";

/* A string-keyed unpack binds as named arguments (php 8.1), and a spread that
 * expands to != 1 element shifts the positions of the named args after it. */
echo $o->d(...[1, 2], ...['p' => 3]), "\n";
echo $o->e(1, ...[2, 3], q: 4), "\n";
$mixed = ['a' => 1, 'b' => 2];
echo $o->f(0, ...$mixed), "\n";

/* Purely positional stays purely positional. */
echo $o->g(1, 2), "\n";
echo $o->h(), "\n";
echo $o->i(...[2 => 'x', 5 => 'y']), "\n";

/* Every CALLABLE spelling of the same call packs the same way. */
$cb = [$o, 'j'];
echo $cb(1, n: 2), "\n";
echo call_user_func([$o, 'k'], 1, n: 2), "\n";
$sb = ['CnakMagic', 'l'];
echo $sb(1, n: 2), "\n";
echo "end\n";
?>
--EXPECT--
a={"0":1,"b":2}
b={"x":1,"y":2,"z":3}
S:c={"0":1,"k":"v"}
d={"0":1,"1":2,"p":3}
e={"0":1,"1":2,"2":3,"q":4}
f={"0":0,"a":1,"b":2}
g=[1,2]
h=[]
i=["x","y"]
j={"0":1,"n":2}
k={"0":1,"n":2}
S:l={"0":1,"n":2}
end
