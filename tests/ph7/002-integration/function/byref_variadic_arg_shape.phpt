--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference variadic tail refuses a non-variable, per collected element
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

function var2(&...$xs) { return 'ret'; }
function mix($first, &...$rest) { return 'm'; }
function t($label, $fn) {
    try { $v = $fn(); echo "$label: "; var_dump($v); }
    catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

$a = 'a'; $b = 'b';
t('variables',     fn() => var2($a, $b));
t('literal',       fn() => var2(1 + 1));
t('second is lit', function () { $v = 'v'; return var2($v, 1 + 1); });
t('call result',   fn() => var2(strtoupper('a')));
t('named',         fn() => var2(xs: 1 + 1));
t('after a fixed param', function () { $v = 'v'; return mix('f', $v); });
t('fixed then lit', fn() => mix('f', 1 + 1));
?>
--EXPECT--
variables: string(3) "ret"
literal: Error: var2(): Argument #1 could not be passed by reference
second is lit: Error: var2(): Argument #2 could not be passed by reference
  [8] Only variables should be passed by reference
call result: string(3) "ret"
named: Error: var2(): Argument #1 could not be passed by reference
after a fixed param: string(1) "m"
fixed then lit: Error: mix(): Argument #2 could not be passed by reference
