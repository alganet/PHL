--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every builtin names php's REASON when a callback argument is not callable
--DESCRIPTION--
php prints `f(): Argument #N ($callback) must be a valid callback, <reason>`,
and the reason names the rule that made the value uncallable. PHL had a
different subset of that taxonomy in each of nine builtins — hand-written
copies in hashmap_builtin.c that answered "function X not found or invalid
function name" for a `'C::m'` string and "class C does not have a method" for
anything about a method it could not call. All of them now come from one
builder, which mirrors is_callable()'s decisions rule for rule, so the
predicate and the reason cannot drift.

php's callback reason reports staticness BEFORE visibility, the reverse of the
direct dispatch (which says "Call to private method" for the same pair); both
orders are pinned.
--FILE--
<?php
class CarC {
    public function m() { return 'm'; }
    private function priv() { return 'priv'; }
    protected function prot() { return 'prot'; }
    public static function s() { return 's'; }
    private static function privStatic() { return 'ps'; }
}
abstract class CarA {
    abstract public function am();
}

function carRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

/* The reason taxonomy, through call_user_func. */
carRun('undefined function', fn() => call_user_func('carNoSuchFunction'));
carRun('empty string', fn() => call_user_func(''));
carRun('class not found', fn() => call_user_func('CarNoSuch::m'));
carRun('missing method str', fn() => call_user_func('CarC::nope'));
carRun('missing method arr', fn() => call_user_func(['CarC', 'nope']));
carRun('private static', fn() => call_user_func('CarC::privStatic'));
carRun('private instance', fn() => call_user_func([new CarC, 'priv']));
carRun('protected instance', fn() => call_user_func([new CarC, 'prot']));
carRun('non-static via class', fn() => call_user_func(['CarC', 'm']));
carRun('non-static private via class', fn() => call_user_func('CarC::priv'));
carRun('abstract', fn() => call_user_func(['CarA', 'am']));
carRun('scope keyword global', fn() => call_user_func('self::m'));
carRun('scope keyword arr global', fn() => call_user_func(['parent', 'm']));
carRun('three members', fn() => call_user_func(['CarC', 's', 'extra']));
carRun('wrong indices', fn() => call_user_func(['a' => 'CarC', 'b' => 's']));
carRun('first member', fn() => call_user_func([5, 's']));
carRun('second member', fn() => call_user_func(['CarC', 5]));
carRun('not a callable at all', fn() => call_user_func(1.5));
carRun('plain object', fn() => call_user_func(new CarC));

/* The same builder answers for every other builtin that takes a callback. */
carRun('usort', function () { $a = [2, 1]; usort($a, ['CarC', 'm']); return $a; });
carRun('uasort', function () { $a = [2, 1]; uasort($a, 'CarC::nope'); return $a; });
carRun('array_map', fn() => array_map(['a' => 'CarC', 'b' => 's'], [1]));
carRun('array_filter', fn() => array_filter([1], [new CarC, 'priv']));
carRun('array_filter empty', fn() => array_filter([], 'carNoSuchFunction'));
carRun('array_reduce', fn() => array_reduce([1], ['CarA', 'am']));
carRun('array_walk', function () { $a = [1]; array_walk($a, ['CarC', 'm']); return $a; });
carRun('array_udiff', fn() => array_udiff([1], [2], ['CarC', 'nope']));
carRun('array_uintersect', fn() => array_uintersect([1], [2], [5, 's']));
carRun('array_diff_uassoc', fn() => array_diff_uassoc([1], [2], 'CarNoSuch::m'));
carRun('array_find', fn() => array_find([1], 'carNoSuchFunction'));
carRun('call_user_func_array', fn() => call_user_func_array([new CarC, 'priv'], []));

/* The DIRECT dispatch keeps its own order: visibility first. */
carRun('direct private', function () { $cb = [new CarC, 'priv']; return $cb(); });
echo "end\n";
?>
--EXPECT--
undefined function => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "carNoSuchFunction" not found or invalid function name
empty string => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "" not found or invalid function name
class not found => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class "CarNoSuch" not found
missing method str => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class CarC does not have a method "nope"
missing method arr => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class CarC does not have a method "nope"
private static => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method CarC::privStatic()
private instance => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method CarC::priv()
protected instance => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access protected method CarC::prot()
non-static via class => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method CarC::m() cannot be called statically
non-static private via class => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method CarC::priv() cannot be called statically
abstract => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot call abstract method CarA::am()
scope keyword global => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "self" when no class scope is active
scope keyword arr global => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when no class scope is active
three members => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback must have exactly two members
wrong indices => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback has to contain indices 0 and 1
first member => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, first array member is not a valid class name or object
second member => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, second array member is not a valid method
not a callable at all => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, no array or string given
plain object => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, no array or string given
usort => TypeError: usort(): Argument #2 ($callback) must be a valid callback, non-static method CarC::m() cannot be called statically
uasort => TypeError: uasort(): Argument #2 ($callback) must be a valid callback, class CarC does not have a method "nope"
array_map => TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, array callback has to contain indices 0 and 1
array_filter => TypeError: array_filter(): Argument #2 ($callback) must be a valid callback or null, cannot access private method CarC::priv()
array_filter empty => TypeError: array_filter(): Argument #2 ($callback) must be a valid callback or null, function "carNoSuchFunction" not found or invalid function name
array_reduce => TypeError: array_reduce(): Argument #2 ($callback) must be a valid callback, cannot call abstract method CarA::am()
array_walk => TypeError: array_walk(): Argument #2 ($callback) must be a valid callback, non-static method CarC::m() cannot be called statically
array_udiff => TypeError: array_udiff(): Argument #3 must be a valid callback, class CarC does not have a method "nope"
array_uintersect => TypeError: array_uintersect(): Argument #3 must be a valid callback, first array member is not a valid class name or object
array_diff_uassoc => TypeError: array_diff_uassoc(): Argument #3 must be a valid callback, class "CarNoSuch" not found
array_find => TypeError: array_find(): Argument #2 ($callback) must be a valid callback, function "carNoSuchFunction" not found or invalid function name
call_user_func_array => TypeError: call_user_func_array(): Argument #1 ($callback) must be a valid callback, cannot access private method CarC::priv()
direct private => Error: Call to private method CarC::priv() from global scope
end
