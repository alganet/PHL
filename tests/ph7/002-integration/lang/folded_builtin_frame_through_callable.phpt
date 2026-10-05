--TEST--
A builtin php folds into an opcode keeps its frame when it is reached through a callable
--FILE--
<?php
// php compiles a LITERAL call to strlen/count/sizeof/array_key_exists/get_class into an
// opcode of its own, so a refusal raised there has no frame for the function. Only the
// compiled call is folded: the same function reached through array_map(), usort(),
// Reflection, a first-class callable or a name held in a variable is a real call, and
// its refusal names it -- as `[internal function]` under another builtin, at the
// calling line otherwise. Two refusals speak by door too: array_key_exists()'s illegal
// key is the opcode's offset Error only when folded, and sizeof() names itself.
function show($label, callable $c) {
    try {
        $c();
        echo "$label: no throw\n";
    } catch (Throwable $e) {
        echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n";
        foreach ($e->getTrace() as $i => $f) {
            if (str_starts_with($f['function'], '{closure')) break;
            echo "  #$i ", isset($f['line']) ? "line {$f['line']}" : '[internal function]', ": ",
                $f['class'] ?? '', $f['type'] ?? '', $f['function'], "()\n";
        }
    }
}
class K {}
set_error_handler(function ($no, $msg) { echo "warning: $msg\n"; return true; });

show('direct strlen', fn() => strlen([]));
show('direct count', fn() => count(1));
show('direct array_key_exists', fn() => array_key_exists([], []));
show('direct sizeof', fn() => sizeof(1));
show('direct sizeof recursion', function () { $a = [1]; $a[] = &$a; echo sizeof($a, COUNT_RECURSIVE), "\n"; });

show('array_map strlen', fn() => array_map('strlen', [[]]));
show('array_map count', fn() => array_map('count', [1]));
show('array_map sizeof', fn() => array_map('sizeof', [1]));
show('array_map get_class', fn() => array_map('get_class', [1]));
show('array_map array_key_exists', fn() => array_map('array_key_exists', [[]], [[]]));
show('usort array_key_exists', function () { $a = [[], []]; usort($a, 'array_key_exists'); });
show('invoke strlen', fn() => (new ReflectionFunction('strlen'))->invoke([]));
show('invokeArgs count', fn() => (new ReflectionFunction('count'))->invokeArgs([1]));
show('fcc strlen', fn() => strlen(...)([]));
show('fcc count', fn() => count(...)(1));
show('Closure::fromCallable strlen', fn() => Closure::fromCallable('strlen')([]));
$n = 'strlen';
show('variable strlen', fn() => $n([]));
$k = 'array_key_exists';
show('variable array_key_exists', fn() => $k(new K, []));
show('fcc array_key_exists', fn() => array_key_exists(...)([], []));
show('fcc get_class ok', function () { echo get_class(...)(new K), "\n"; });
--EXPECT--
direct strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
direct count: TypeError: count(): Argument #1 ($value) must be of type Countable|array, int given
direct array_key_exists: TypeError: Cannot access offset of type array on array
direct sizeof: TypeError: sizeof(): Argument #1 ($value) must be of type Countable|array, int given
warning: sizeof(): Recursion detected
2
direct sizeof recursion: no throw
array_map strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
  #0 [internal function]: strlen()
  #1 line 31: array_map()
array_map count: TypeError: count(): Argument #1 ($value) must be of type Countable|array, int given
  #0 [internal function]: count()
  #1 line 32: array_map()
array_map sizeof: TypeError: sizeof(): Argument #1 ($value) must be of type Countable|array, int given
  #0 [internal function]: sizeof()
  #1 line 33: array_map()
array_map get_class: TypeError: get_class(): Argument #1 ($object) must be of type object, int given
  #0 [internal function]: get_class()
  #1 line 34: array_map()
array_map array_key_exists: TypeError: array_key_exists(): Argument #1 ($key) must be a valid array offset type
  #0 [internal function]: array_key_exists()
  #1 line 35: array_map()
usort array_key_exists: TypeError: array_key_exists(): Argument #1 ($key) must be a valid array offset type
  #0 [internal function]: array_key_exists()
  #1 line 36: usort()
invoke strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
  #0 [internal function]: strlen()
  #1 line 37: ReflectionFunction->invoke()
invokeArgs count: TypeError: count(): Argument #1 ($value) must be of type Countable|array, int given
  #0 [internal function]: count()
  #1 line 38: ReflectionFunction->invokeArgs()
fcc strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
  #0 line 39: strlen()
fcc count: TypeError: count(): Argument #1 ($value) must be of type Countable|array, int given
  #0 line 40: count()
Closure::fromCallable strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
  #0 line 41: strlen()
variable strlen: TypeError: strlen(): Argument #1 ($string) must be of type string, array given
  #0 line 43: strlen()
variable array_key_exists: TypeError: array_key_exists(): Argument #1 ($key) must be a valid array offset type
  #0 line 45: array_key_exists()
fcc array_key_exists: TypeError: array_key_exists(): Argument #1 ($key) must be a valid array offset type
  #0 line 46: array_key_exists()
K
fcc get_class ok: no throw
