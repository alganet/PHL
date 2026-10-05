--TEST--
A namespaced call in the arguments of another that php makes frameless is never frameless itself: what it calls back binds weakly
--FILE--
<?php
declare(strict_types=1);
namespace N;
/* An unqualified builtin call inside a namespace that php 8.4 could make FRAMELESS
 * is compiled as two branches, and to keep them from nesting, php never makes a
 * namespaced call written anywhere in its arguments frameless. That inner call is
 * then an ordinary internal one: what it calls back binds weakly. A closure body in
 * the arguments is its own function, and a fully-qualified inner call, an outer
 * call that is not frameless-shaped, or an imported outer name leave it alone. */
use function in_array as ia;
set_error_handler(function (string $no, string $str) { echo "  handler got ", var_export($no, true), "\n"; return true; });
spl_autoload_register(function (int $c) { echo "  autoload\n"; });
class O { function m($x) { return $x; } }
function run($l, $f) {
    echo $l, "\n";
    try { $f(); } catch (\TypeError $e) { echo '  TypeError ', \preg_replace('/\{closure:[^}]*\}/', '{closure}', \str_replace(__FILE__, 'F', $e->getMessage())), "\n"; }
}
run('plain', function () { preg_match('/(', 'x'); });
run('nested in max', function () { max(preg_match('/(', 'x'), 1); });
run('nested deeper', function () { max((int)abs(preg_match('/(', 'x')), 1); });
run('nested in in_array', function () { in_array(preg_match('/(', 'x'), [1]); });
run('nested, spelled upper', function () { MAX(Preg_Match('/(', 'x'), 1); });
run('both arguments', function () { max(preg_match('/(', 'x'), preg_match('/(', 'x')); });
run('under a method call', function () { max((new O)->m(preg_match('/(', 'x')), 1); });
run('under a ternary', function () { max(true ? preg_match('/(', 'x') : 0, 1); });
run('autoloader, nested', function () { max((int)class_exists('Nope1'), 1); });
run('autoloader, plain', function () { class_exists('Nope2'); });
run('after the outer call', function () { max(1, 2); preg_match('/(', 'x'); });
run('outer not frameless', function () { abs(preg_match('/(', 'x')); });
run('outer at another arity', function () { max(preg_match('/(', 'x'), 1, 2); });
run('outer fully qualified', function () { \max(preg_match('/(', 'x'), 1); });
run('outer imported', function () { ia(preg_match('/(', 'x'), [1]); });
run('outer named argument', function () { in_array(preg_match('/(', 'x'), haystack: [1]); });
run('outer spread', function () { max(...[preg_match('/(', 'x'), 1]); });
run('inner fully qualified', function () { max(\preg_match('/(', 'x'), 1); });
run('closure in the arguments', function () { max((function () { return preg_match('/(', 'x'); })(), 1); });
run('arrow fn in the arguments', function () { max((fn() => preg_match('/(', 'x'))(), 1); });
run('anonymous class in the arguments', function () { max((new class { function m() { return preg_match('/(', 'x'); } })->m(), 1); });
--EXPECT--
plain
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 18
nested in max
  handler got '2'
nested deeper
  handler got '2'
  TypeError abs(): Argument #1 ($num) must be of type int|float, false given
nested in in_array
  handler got '2'
nested, spelled upper
  handler got '2'
both arguments
  handler got '2'
  handler got '2'
under a method call
  handler got '2'
under a ternary
  handler got '2'
autoloader, nested
  TypeError {closure}(): Argument #1 ($c) must be of type int, string given
autoloader, plain
  TypeError {closure}(): Argument #1 ($c) must be of type int, string given, called in F on line 27
after the outer call
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 28
outer not frameless
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 29
outer at another arity
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 30
outer fully qualified
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 31
outer imported
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 32
outer named argument
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 33
outer spread
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 34
inner fully qualified
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 35
closure in the arguments
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 36
arrow fn in the arguments
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 37
anonymous class in the arguments
  TypeError {closure}(): Argument #1 ($no) must be of type string, int given, called in F on line 38
