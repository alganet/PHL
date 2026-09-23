--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.5 clone() is a real internal function: FCC, indirect calls, runtime arity
--FILE--
<?php
class CloneFnZ {
    public $n = 1;
    private $p = 'x';
    public readonly float $r;
    public function __construct() { $this->r = 1.0; }
    public function with(float $r): static { return clone($this, ['r' => $r]); }
    public function withP($v): static { return clone($this, ['p' => $v]); }
}
$z = new CloneFnZ;
function cloneFnTry(callable $f) {
    try { var_dump($f()); } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

// it is a function, and it is internal
var_dump(function_exists('clone'), is_callable('clone'));
var_dump(in_array('clone', get_defined_functions()['internal'], true));
$rf = new ReflectionFunction('clone');
echo $rf->getName(), '(', implode(', ', array_map(
    fn($p) => (string)$p->getType() . ' $' . $p->getName(), $rf->getParameters())),
    '): ', (string)$rf->getReturnType(), "\n";
var_dump($rf->isInternal(), $rf->getNumberOfRequiredParameters(), $rf->getNumberOfParameters());

// every indirect spelling reaches it
$fcc = clone(...);
var_dump($fcc instanceof Closure, get_class($fcc($z)));
$name = 'clone';
var_dump($name($z)->n);
var_dump(call_user_func('clone', $z)->n);
$args = [$z, ['n' => 9]];
$spread = clone(...$args);
var_dump($spread->n);
$cufa = call_user_func_array('clone', $args);
var_dump($cufa->n);

// a degenerate argument list is a RUNTIME error, like php's
cloneFnTry(fn() => clone());
cloneFnTry(fn() => clone($z, ['n' => 1], 3));
cloneFnTry(fn() => clone($z, 'notanarray'));
cloneFnTry(fn() => clone(5));
cloneFnTry(fn() => clone(nope: $z));

// the property updates keep the CALLING scope's rights
cloneFnTry(fn() => $z->with(2.5)->r);
cloneFnTry(fn() => $z->withP('q')->n);
cloneFnTry(function () use ($z) { $c = clone($z, ['p' => 'outside']); return $c->n; });
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
clone(object $object, array $withProperties): object
bool(true)
int(1)
int(2)
bool(true)
string(8) "CloneFnZ"
int(1)
int(1)
int(9)
int(9)
ArgumentCountError: clone() expects at least 1 argument, 0 given
ArgumentCountError: clone() expects at most 2 arguments, 3 given
TypeError: clone(): Argument #2 ($withProperties) must be of type array, string given
TypeError: clone(): Argument #1 ($object) must be of type object, int given
Error: Unknown named parameter $nope
float(2.5)
int(1)
Error: Cannot access private property CloneFnZ::$p
