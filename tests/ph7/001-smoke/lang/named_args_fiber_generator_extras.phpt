--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments to Fiber::start() and to a generator's variadic keep their names
--FILE--
<?php
function nafg_show(string $label, callable $f) {
    try {
        $f();
    } catch (Throwable $e) {
        echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
function nafg_gen($a, $b = 5, ...$r) {
    echo "gen: ", json_encode([$a, $b, $r, func_num_args()]), "\n";
    yield 1;
}
function nafg_gen_req($a, $b, ...$r) {
    yield 1;
}
function nafg_fib($a, $b = 5, ...$r) {
    echo "fiber: ", json_encode([$a, $b, $r, func_num_args()]), "\n";
}
function nafg_fib_req($a, $b, ...$r) {
}
class NafgBody {
    function m(...$r) { echo "method: ", json_encode($r), "\n"; }
    static function s($a, $b = 2) { echo "static: ", json_encode([$a, $b]), "\n"; }
}

// A generator's unknown-name extra is keyed in its variadic, and never binds
// the defaulted formal it skips.
foreach (nafg_gen(1, zz: 2) as $_);
foreach (nafg_gen(b: 7, a: 1, zz: 2, yy: 4) as $_);
foreach (nafg_gen(1, ...['x' => 3]) as $_);
nafg_show("gen hole", function () { foreach (nafg_gen_req(b: 1, zz: 2) as $_); });

// Fiber::start() forwards names: they bind by name, and the extras are keyed.
(new Fiber('nafg_fib'))->start(1, zz: 2);
(new Fiber('nafg_fib'))->start(b: 3, a: 1, zz: 2);
(new Fiber('nafg_fib'))->start(...['b' => 8, 'a' => 9]);
(new Fiber(function (...$a) { echo "closure: ", json_encode($a), "\n"; }))->start(1, zz: 2);
(new Fiber(function ($a, $b) { echo "swap: ", json_encode([$a, $b]), "\n"; }))->start(b: 1, a: 2);
(new Fiber([new NafgBody, 'm']))->start(p: 1);
(new Fiber('NafgBody::s'))->start(b: 9, a: 1);
nafg_show("overwrite", function () { (new Fiber(function ($a) {}))->start(1, a: 2); });
nafg_show("unknown", function () { (new Fiber(function ($a) {}))->start(q: 2); });
nafg_show("fiber hole", function () { (new Fiber('nafg_fib_req'))->start(b: 1, zz: 2); });
--EXPECT--
gen: [1,5,{"zz":2},1]
gen: [1,7,{"zz":2,"yy":4},2]
gen: [1,5,{"x":3},1]
gen hole: ArgumentCountError: nafg_gen_req(): Argument #1 ($a) not passed
fiber: [1,5,{"zz":2},1]
fiber: [1,3,{"zz":2},2]
fiber: [9,8,[],2]
closure: {"0":1,"zz":2}
swap: [2,1]
method: {"p":1}
static: [1,9]
overwrite: Error: Named parameter $a overwrites previous argument
unknown: Error: Unknown named parameter $q
fiber hole: ArgumentCountError: nafg_fib_req(): Argument #1 ($a) not passed
