--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference parameter refuses an argument that is not a variable
--FILE--
<?php
class C {
    const K = [3, 1];
    public static $s = [3, 1];
    public $p = [3, 1];
    public function m(&$x) { $x = 'W'; return 'ok'; }
    public static function sm(&$x) { $x = 'W'; return 'ok'; }
}
function r(&$x) { $x = 'W'; return 'ok'; }
function t($label, $fn) {
    try { $fn(); echo "$label: bound\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "== shapes php BINDS ==\n";
$o = new C; $a = ['k' => [3, 1]]; $v = [3, 1]; $n = 'v';
t('$var',        function () { $q = [3, 1]; return sort($q); });
t('$$var',       function () { $v = [3, 1]; $n = 'v'; return sort($$n); });
t('${expr}',     function () { $v = [3, 1]; return sort(${'v'}); });
t('($var)',      function () { $q = [3, 1]; return sort(($q)); });
t('$a[k]',       function () { $a = ['k' => [3, 1]]; return sort($a['k']); });
t('$o->p',       function () { $o = new C; return sort($o->p); });
t('C::$s',       function () { return sort(C::$s); });

echo "== shapes php REFUSES ==\n";
t('literal',     fn() => sort([3, 1]));
t('C::K',        fn() => sort(C::K));
t('(array)$v',   function () { $v = [3, 1]; return sort((array)$v); });
t('@$undef',     fn() => sort(@$undef));
t('$o?->p',      function () { $o = new C; return sort($o?->p); });
t('ternary',     fn() => sort(true ? [3, 1] : []));
t('assignment',  fn() => sort($q = [3, 1]));

echo "== the refusal names the method's class ==\n";
t('instance',    fn() => (new C)->m(1 + 1));
t('static',      fn() => C::sm(1 + 1));
t('function',    fn() => r(1 + 1));

echo "== builtins with a & row, not just the five that self-checked ==\n";
t('usort',       fn() => usort('x', fn($p, $q) => 0));
t('preg_match',  fn() => preg_match('/a/', 'abc', 'lit'));
t('shuffle',     fn() => shuffle([1, 2]));
t('reset',       fn() => reset([1, 2]));
t('array_splice', fn() => array_splice([1, 2], 0));
t('settype',     fn() => settype([1, 2], 'string'));
t('array_pop',   fn() => array_pop([1, 2]));

echo "== the refusal beats the too-MANY-arguments check, not the too-FEW one ==\n";
t('extra args',  fn() => array_pop([1, 2], 5));
t('no args',     fn() => array_pop());

echo "== an operator result no longer ALIASES its left operand ==\n";
$i = 5;
try { r($i + 1); } catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump($i);
$s = 'abc';
try { r($s . 'y'); } catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump($s);

echo "== a by-ref argument that IS a variable still binds and writes back ==\n";
$w = 'before';
r($w);
var_dump($w);
$arr = [3, 1, 2];
sort($arr);
var_dump($arr);
?>
--EXPECT--
== shapes php BINDS ==
$var: bound
$$var: bound
${expr}: bound
($var): bound
$a[k]: bound
$o->p: bound
C::$s: bound
== shapes php REFUSES ==
literal: Error: sort(): Argument #1 ($array) could not be passed by reference
C::K: Error: sort(): Argument #1 ($array) could not be passed by reference
(array)$v: Error: sort(): Argument #1 ($array) could not be passed by reference
@$undef: Error: sort(): Argument #1 ($array) could not be passed by reference
$o?->p: Error: sort(): Argument #1 ($array) could not be passed by reference
ternary: Error: sort(): Argument #1 ($array) could not be passed by reference
assignment: Error: sort(): Argument #1 ($array) could not be passed by reference
== the refusal names the method's class ==
instance: Error: C::m(): Argument #1 ($x) could not be passed by reference
static: Error: C::sm(): Argument #1 ($x) could not be passed by reference
function: Error: r(): Argument #1 ($x) could not be passed by reference
== builtins with a & row, not just the five that self-checked ==
usort: Error: usort(): Argument #1 ($array) could not be passed by reference
preg_match: Error: preg_match(): Argument #3 ($matches) could not be passed by reference
shuffle: Error: shuffle(): Argument #1 ($array) could not be passed by reference
reset: Error: reset(): Argument #1 ($array) could not be passed by reference
array_splice: Error: array_splice(): Argument #1 ($array) could not be passed by reference
settype: Error: settype(): Argument #1 ($var) could not be passed by reference
array_pop: Error: array_pop(): Argument #1 ($array) could not be passed by reference
== the refusal beats the too-MANY-arguments check, not the too-FEW one ==
extra args: Error: array_pop(): Argument #1 ($array) could not be passed by reference
no args: ArgumentCountError: array_pop() expects exactly 1 argument, 0 given
== an operator result no longer ALIASES its left operand ==
Error
int(5)
Error
string(3) "abc"
== a by-ref argument that IS a variable still binds and writes back ==
string(1) "W"
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
