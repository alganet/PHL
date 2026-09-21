--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Function names are case-insensitive at every call and lookup site
--FILE--
<?php
function fnciTag(string $who = 'user'): string { return "tag($who)"; }

// Direct calls, on a builtin and on a user function, in any spelling.
echo STRLEN('abcd'), StrLen('abc'), strlen('ab'), ' ', FNCITAG(), fnCiTag(), "\n";

// Variable, string-callable and array-callable dispatch.
$fnciVar = 'FNCITAG';
$fnciBuiltin = 'STRTOUPPER';
echo $fnciVar(), ' ', $fnciBuiltin('up'), ' ', call_user_func('FnCiTag'), ' ',
    call_user_func_array('STRTOLOWER', ['DOWN']), "\n";

// Builtins that take a callback resolve it the same way.
print_r(array_map('STRTOUPPER', ['a', 'b']));
$fnciList = [3, 1, 2];
usort($fnciList, 'FNCICMP');
function fnciCmp($a, $b): int { return $a <=> $b; }
echo implode(',', $fnciList), ' ', implode(',', array_filter([0, 1, 2], 'BOOLVAL')), "\n";

// Predicates and Reflection agree with the call.
echo var_export(function_exists('STRLEN'), true), var_export(function_exists('FnCiTag'), true),
    var_export(is_callable('STRLEN'), true), var_export(is_callable('FNCITAG'), true), "\n";
echo (new ReflectionFunction('fncitag'))->getName(), ' ',
    (new ReflectionFunction('FNCITAG'))->getNumberOfParameters(), "\n";

// The DECLARED spelling is what the engine reports back, everywhere.
function fnciWhoAmI(): string { return __FUNCTION__; }
echo FNCIWHOAMI(), ' ', (new ReflectionFunction('FNCIWHOAMI'))->getName(), "\n";

// A wrong-case name that exists nowhere is still an ordinary undefined function.
echo var_export(function_exists('FNCI_NO_SUCH_FUNCTION'), true), "\n";
try {
    $fnciMissing = 'FNCI_NO_SUCH_FUNCTION';
    $fnciMissing();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
432 tag(user)tag(user)
tag(user) UP tag(user) down
Array
(
    [0] => A
    [1] => B
)
1,2,3 1,2
truetruetruetrue
fnciTag 1
fnciWhoAmI fnciWhoAmI
false
Call to undefined function FNCI_NO_SUCH_FUNCTION()
--CLEAN--
<?php
