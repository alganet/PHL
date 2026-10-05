--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named call counts the defaulted holes below the last formal it binds
--FILE--
<?php
// A named call's arity runs up to the highest formal a name binds, and a hole
// below it -- filled by its default -- is an argument: func_num_args(),
// func_get_args(), func_get_arg() and debug_backtrace()'s args all see it.
function nahca_show($l, $args, $n) { echo $l, ": ", $n, " ", json_encode($args), "\n"; }
function nahca_f($a = 1, $b = 2, $c = 5) {
    nahca_show('f', func_get_args(), func_num_args());
    try { var_dump(func_get_arg(1)); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
}
nahca_f(b: 3); nahca_f(c: 9); nahca_f(7, c: 9); nahca_f(a: 4); nahca_f(); nahca_f(1); nahca_f(c: 1, a: 2);
nahca_f(...['b' => 3]); nahca_f(1, ...['c' => 3]);
// A named extra the variadic collects is no positional argument; the
// backtrace alone shows it, under its name.
function nahca_v($a = 1, $b = 2, ...$r) {
    nahca_show('v', func_get_args(), func_num_args());
    echo json_encode(debug_backtrace()[0]['args']), "\n";
}
nahca_v(b: 3, z: 4); nahca_v(a: 1, z: 4); nahca_v(1, 2, 3, z: 4); nahca_v(z: 4); nahca_v(b: 3);
// func_get_arg() reads the variadic's elements, not its packed array.
function nahca_w($a = 1, ...$r) {
    try { var_dump(func_get_arg(2)); } catch (ValueError $e) { echo "ValueError\n"; }
}
nahca_w(1, 2, 3); nahca_w(1, 2, q: 3);
class NahcaK {
    function __construct($a = 1, $b = 2) { nahca_show('ctor', func_get_args(), func_num_args()); }
    function m($a = 1, $b = 2) { nahca_show('m', func_get_args(), func_num_args()); }
    static function s($a = 1, $b = 2) { nahca_show('s', func_get_args(), func_num_args()); }
}
new NahcaK(b: 4); (new NahcaK)->m(b: 8); NahcaK::s(b: 8);
function nahca_r(&$a = null, $b = 2, &$c = 0) { nahca_show('r', func_get_args(), func_num_args()); $c = 7; }
$nahcaZ = 1; nahca_r(c: $nahcaZ); var_dump($nahcaZ);
call_user_func('nahca_f', b: 6); call_user_func_array('nahca_f', [1, 'c' => 2]);
$nahcaC = function ($a = 1, $b = 2) { nahca_show('closure', func_get_args(), func_num_args()); };
$nahcaC(b: 5);
(nahca_f(...))(b: 0);
function nahca_g($a = 1, $b = 2) { yield func_get_args(); yield func_num_args(); }
foreach (nahca_g(b: 3) as $nahcaX) echo json_encode($nahcaX), "\n";
?>
--EXPECT--
f: 2 [1,3]
int(3)
f: 3 [1,2,9]
int(2)
f: 3 [7,2,9]
int(2)
f: 1 [4]
func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function
f: 0 []
func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function
f: 1 [1]
func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function
f: 3 [2,2,1]
int(2)
f: 2 [1,3]
int(3)
f: 3 [1,2,3]
int(2)
v: 2 [1,3]
{"0":1,"1":3,"z":4}
v: 1 [1]
{"0":1,"z":4}
v: 3 [1,2,3]
{"0":1,"1":2,"2":3,"z":4}
v: 0 []
{"z":4}
v: 2 [1,3]
[1,3]
int(3)
ValueError
ctor: 2 [1,4]
ctor: 0 []
m: 2 [1,8]
s: 2 [1,8]
r: 3 [null,2,1]
int(7)
f: 2 [1,6]
int(6)
f: 3 [1,2,2]
int(2)
closure: 2 [1,5]
f: 2 [1,0]
int(0)
[1,3]
2
