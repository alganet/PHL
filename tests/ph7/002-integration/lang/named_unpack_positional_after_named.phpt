--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A positional argument after a named one is refused only inside the same unpack
--FILE--
<?php
/* php forgets a named argument at the end of each unpack: a positional element
 * of a LATER unpack is legal, and it binds after the highest parameter a name
 * filled, so a hole the name jumped over stays a hole (a default, or `not
 * passed`). Inside ONE unpack the order is still refused. A variadic holds every
 * positional element before any named one, whichever unpack wrote it first. */
function npa_f($a = 'da', $b = 'db', $c = 'dc') { return json_encode(func_get_args()) . " a=$a b=$b c=$c"; }
function npa_g($a = 'da', ...$x) { return json_encode(func_get_args()) . " a=$a x=" . json_encode($x); }
function npa_h($a, $b) { return "a=$a b=$b"; }
function npa_t(int $a, int ...$x) { return $x; }
function npa_gen($a = 'da', ...$x) { yield [$a, $x]; }
function npa_it($arr) { foreach ($arr as $k => $v) yield $k => $v; }
class NpaC {
    public $v;
    function __construct($a = 'da', $b = 'db', ...$x) { $this->v = [func_get_args(), $a, $b, $x]; }
    function m($a = 'da', $b = 'db') { return [func_get_args(), $a, $b]; }
}
$cases = [
    'f a|2'        => fn() => npa_f(...['a' => 1], ...[2]),
    'f b|2'        => fn() => npa_f(...['b' => 1], ...[2]),
    'f c|2'        => fn() => npa_f(...['c' => 1], ...[2]),
    'f a|2,3,4'    => fn() => npa_f(...['a' => 1], ...[2, 3, 4]),
    'f 1|b|3'      => fn() => npa_f(...[1], ...['b' => 2], ...[3]),
    'f b|a|3'      => fn() => npa_f(...['b' => 1], ...['a' => 2], ...[3]),
    'f b|2,3'      => fn() => npa_f(...['b' => 1], ...[2, 3]),
    'f a,2 (one)'  => fn() => npa_f(...['a' => 1, 2]),
    'f a|[]'       => fn() => npa_f(...['a' => 1], ...[]),
    'g q|2,3'      => fn() => npa_g(...['q' => 1], ...[2, 3]),
    'g 0|q|2'      => fn() => npa_g(...[0], ...['q' => 1], ...[2]),
    'h b|2'        => fn() => npa_h(...['b' => 1], ...[2]),
    'iter b|2'     => fn() => npa_f(...npa_it(['b' => 1]), ...[2]),
    'typed q|2,z'  => fn() => npa_t(...['q' => 1], ...[2, 'z']),
    'new q|2,3,4'  => fn() => (new NpaC(...['q' => 1], ...[2, 3, 4]))->v,
    'method b|2'   => fn() => (new NpaC)->m(...['b' => 1], ...[2]),
    'gen q|2,3'    => fn() => npa_gen(...['q' => 1], ...[2, 3])->current(),
    'fiber q|2,3'  => function () {
        $fb = new Fiber(function ($a = 'da', ...$x) { return [$a, $x]; });
        $fb->start(...['q' => 1], ...[2, 3]);
        return $fb->getReturn();
    },
    'refl q|2,3'   => fn() => (new ReflectionClass('NpaC'))->newInstance(...['q' => 1], ...[2, 3])->v,
    'str_pad s|-'  => fn() => str_pad(...['string' => 'x'], ...[5, '-']),
    'str_pad l|x'  => fn() => str_pad(...['length' => 5], ...['x']),
    'str_pad (one)'=> fn() => str_pad(...['string' => 'x', 5]),
    'str_pad over' => fn() => str_pad(...['string' => 'x'], ...[5, '-', 0, 9]),
    'array_merge'  => fn() => array_merge(...['a' => [1]], ...[[2]]),
    'cuf fwd'      => fn() => call_user_func('str_pad', ...['pad_string' => '-'], ...['x', 5]),
    'cuf closure'  => fn() => call_user_func(fn(...$a) => $a, ...['q' => 1], ...[2]),
    'cuf overwrite'=> fn() => call_user_func('npa_f', ...['a' => 1], ...[2]),
    'cufa'         => fn() => call_user_func_array('str_pad', ['string' => 'x', 5]),
];
foreach ($cases as $k => $c) {
    try {
        $r = $c();
        echo "$k: ", is_string($r) ? $r : json_encode($r), "\n";
    } catch (Throwable $e) {
        echo "$k: ", get_class($e), ": ", strtok($e->getMessage(), ','), "\n";
    }
}
--EXPECT--
f a|2: [1,2] a=1 b=2 c=dc
f b|2: ["da",1,2] a=da b=1 c=2
f c|2: ["da","db",1,2] a=da b=db c=1
f a|2,3,4: [1,2,3,4] a=1 b=2 c=3
f 1|b|3: [1,2,3] a=1 b=2 c=3
f b|a|3: [2,1,3] a=2 b=1 c=3
f b|2,3: ["da",1,2,3] a=da b=1 c=2
f a,2 (one): Error: Cannot use positional argument after named argument during unpacking
f a|[]: [1] a=1 b=db c=dc
g q|2,3: [2,3] a=2 x={"0":3,"q":1}
g 0|q|2: [0,2] a=0 x={"0":2,"q":1}
h b|2: ArgumentCountError: npa_h(): Argument #1 ($a) not passed
iter b|2: ["da",1,2] a=da b=1 c=2
typed q|2,z: TypeError: npa_t(): Argument #2 must be of type int
new q|2,3,4: [[2,3,4],2,3,{"0":4,"q":1}]
method b|2: [["da",1,2],"da",1]
gen q|2,3: [2,{"0":3,"q":1}]
fiber q|2,3: [2,{"0":3,"q":1}]
refl q|2,3: [[2,3],2,3,{"q":1}]
str_pad s|-: x----
str_pad l|x: ArgumentCountError: str_pad(): Argument #1 ($string) not passed
str_pad (one): Error: Cannot use positional argument after named argument during unpacking
str_pad over: ArgumentCountError: str_pad() expects at most 4 arguments
array_merge: ArgumentCountError: array_merge() does not accept unknown named parameters
cuf fwd: x----
cuf closure: {"0":2,"q":1}
cuf overwrite: Error: Named parameter $a overwrites previous argument
cufa: Error: Cannot use positional argument after named argument
