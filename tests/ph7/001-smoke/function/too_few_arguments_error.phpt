--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Too few arguments to a user function throws ArgumentCountError (functions, methods, ctors, generators, closures, fibers, named args)
--FILE--
<?php
function tfaPlain(int $x) { return $x; }
function tfaOpt($a, $b = 2) {}
function tfaVariadic($a, ...$rest) {}
class TfaC {
    public function __construct($a) {}
    public function m($a, $b) {}
}
function tfaGen($a) { yield $a; }

try { tfaPlain(); } catch (ArgumentCountError $e) { echo get_class($e), "|", $e->getMessage(), "\n"; }
echo (new ReflectionClass("ArgumentCountError"))->getParentClass()->getName(), "\n";
try { tfaOpt(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { tfaVariadic(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { (new TfaC(1))->m(1); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { new TfaC(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { tfaGen(); } catch (ArgumentCountError $e) { echo "gen-eager|", $e->getMessage(), "\n"; }
try { tfaOpt(b: 3); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { call_user_func('tfaPlain'); } catch (ArgumentCountError $e) { echo "cuf|", $e->getMessage(), "\n"; }
$tfaClosure = function ($a) { return $a; };
try { $tfaClosure(); } catch (ArgumentCountError $e) { echo "closure|", $e->getMessage(), "\n"; }
try { (new Fiber(function ($a) {}))->start(); } catch (ArgumentCountError $e) { echo "fiber|", $e->getMessage(), "\n"; }
// Named hole with nothing filled above it: php keeps the positional count wording
function tfaNm($a, $b, $c = 3) {}
try { tfaNm(a: 1); } catch (ArgumentCountError $e) { echo "nm-above|", $e->getMessage(), "\n"; }
// Internal-class ctor: php's ZPP wording, no call-site segment
try { new ReflectionProperty(); } catch (ArgumentCountError $e) { echo "internal|", $e->getMessage(), "\n"; }
// RECV order: a type error on a PASSED argument beats the count error
function tfaRecv(int $x, $y) {}
try { tfaRecv("str"); } catch (TypeError $e) { echo "recv-order|", get_class($e), "\n"; }
echo tfaOpt(1) ?? "null-ok", "|", tfaPlain(7), "\n";
?>
--EXPECTF--
ArgumentCountError|Too few arguments to function tfaPlain(), 0 passed in %s on line %d and exactly 1 expected
TypeError
Too few arguments to function tfaOpt(), 0 passed in %s on line %d and at least 1 expected
Too few arguments to function tfaVariadic(), 0 passed in %s on line %d and exactly 1 expected
Too few arguments to function TfaC::m(), 1 passed in %s on line %d and exactly 2 expected
Too few arguments to function TfaC::__construct(), 0 passed in %s on line %d and exactly 1 expected
gen-eager|Too few arguments to function tfaGen(), 0 passed in %s on line %d and exactly 1 expected
tfaOpt(): Argument #1 ($a) not passed
cuf|Too few arguments to function tfaPlain(), 0 passed in %s on line %d and exactly 1 expected
closure|Too few arguments to function %s 0 passed in %s on line %d and exactly 1 expected
fiber|Too few arguments to function %s 0 passed and exactly 1 expected
nm-above|Too few arguments to function tfaNm(), 1 passed in %s on line %d and at least 2 expected
internal|ReflectionProperty::__construct() expects exactly 2 arguments, 0 given
recv-order|TypeError
null-ok|7
--CLEAN--
<?php
unset($tfaClosure, $e);
