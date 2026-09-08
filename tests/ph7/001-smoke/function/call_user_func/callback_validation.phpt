--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An invalid callback argument is a TypeError, not a silent no-op
--FILE--
<?php
// php validates a callback ARGUMENT before calling anything. PH7 handed it to a
// dispatcher that answers NULL in silence, so call_user_func() returned NULL and
// usort() reordered the array by nothing at all.
class CbC { public function m() { return 'm'; } }
function cbTry($label, $fn)
{
    try {
        echo $label, ' => ', var_export($fn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
cbTry('cuf missing',    fn() => call_user_func('cb_no_such_fn'));
cbTry('cuf bad method', fn() => call_user_func([new CbC, 'nope']));
cbTry('cuf bad class',  fn() => call_user_func(['CbNoCls', 'm']));
cbTry('cuf one elem',   fn() => call_user_func([1]));
cbTry('cuf int',        fn() => call_user_func(5));
cbTry('cufa missing',   fn() => call_user_func_array('cb_no_such_fn', []));
cbTry('usort bad',      function () { $a = [3, 1]; usort($a, 'cb_no_such_cmp'); return $a; });
cbTry('uasort bad',     function () { $a = [3, 1]; uasort($a, 'cb_no_such_cmp'); return $a; });
cbTry('uksort bad',     function () { $a = [3, 1]; uksort($a, 'cb_no_such_cmp'); return $a; });
cbTry('array_walk bad', function () { $a = [1]; array_walk($a, 'cb_no_such_fn'); return $a; });
cbTry('array_map bad',  fn() => array_map('cb_no_such_fn', [1]));

// Valid callbacks, and the sort flags that are NOT callbacks, still work.
cbTry('cuf ok',      fn() => call_user_func([new CbC, 'm']));
cbTry('usort ok',    function () { $a = [3, 1, 2]; usort($a, fn($x, $y) => $x <=> $y); return $a; });
cbTry('sort flags',  function () { $a = ['b', 'a']; sort($a, SORT_STRING); return $a; });
cbTry('rsort flags', function () { $a = [1, 3]; rsort($a, SORT_NUMERIC); return $a; });
?>
--EXPECT--
cuf missing => cuf missing => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "cb_no_such_fn" not found or invalid function name
cuf bad method => cuf bad method => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class CbC does not have a method "nope"
cuf bad class => cuf bad class => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class "CbNoCls" not found
cuf one elem => cuf one elem => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback must have exactly two members
cuf int => cuf int => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, no array or string given
cufa missing => cufa missing => TypeError: call_user_func_array(): Argument #1 ($callback) must be a valid callback, function "cb_no_such_fn" not found or invalid function name
usort bad => usort bad => TypeError: usort(): Argument #2 ($callback) must be a valid callback, function "cb_no_such_cmp" not found or invalid function name
uasort bad => uasort bad => TypeError: uasort(): Argument #2 ($callback) must be a valid callback, function "cb_no_such_cmp" not found or invalid function name
uksort bad => uksort bad => TypeError: uksort(): Argument #2 ($callback) must be a valid callback, function "cb_no_such_cmp" not found or invalid function name
array_walk bad => array_walk bad => TypeError: array_walk(): Argument #2 ($callback) must be a valid callback, function "cb_no_such_fn" not found or invalid function name
array_map bad => array_map bad => TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "cb_no_such_fn" not found or invalid function name
cuf ok => 'm'
usort ok => array (
  0 => 1,
  1 => 2,
  2 => 3,
)
sort flags => array (
  0 => 'a',
  1 => 'b',
)
rsort flags => array (
  0 => 3,
  1 => 1,
)
--CLEAN--
<?php
