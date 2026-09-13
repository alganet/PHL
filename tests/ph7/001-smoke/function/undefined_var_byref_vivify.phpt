--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Undefined variable passed to a user-function by-ref param vivifies and writes back
--FILE--
<?php
// Undefined variable passed to a USER-function by-ref param vivifies + writes back.
function setIt(&$x) { $x = 5; }
setIt($a);
echo "out-param: {$a}\n";

// Nested by-ref forwarding through undefined variables.
function uvbv_inner(&$x) { $x = 1; }
function outer(&$y) { uvbv_inner($y); }
outer($b);
echo "nested: {$b}\n";

// By-ref out-param that builds an array.
function fill(&$x) { $x = []; $x[] = 9; }
fill($c);
echo "array out-param: {$c[0]}\n";

// The PHPUnit ReturnReference shape: undefined var -> by-ref ctor param -> property ref.
class Ref { public mixed $v; function __construct(&$r) { $this->v = &$r; } }
function make(&$x) { return new Ref($x); }
$stub = make($value);   // $value is undefined here
$value = true;          // assigned after binding
var_dump($stub->v);
?>
--EXPECT--
out-param: 5
nested: 1
array out-param: 9
bool(true)
--CLEAN--
<?php
unset($a, $b, $c, $stub, $value);
