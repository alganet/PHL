--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
method_exists/property_exists/get_parent_class refuse what names no class
--DESCRIPTION--
php screens the first argument of all three in the function body, so the file
mode does not matter. method_exists() and property_exists() take any object or
any string -- a string naming nothing is FALSE -- and refuse every other type
with "must be of type object|string". get_parent_class() is stricter: its
argument must be an object or the name of a class that exists, so an int, null,
'' and a typo are all one TypeError, "must be an object or a valid class name".
All three answered FALSE for an int, a float or a bool, and get_parent_class()
answered FALSE for a string naming nothing.
--FILE--
<?php
class CmpBase { public $p; function m() {} }
class CmpLeaf extends CmpBase {}
$cmpShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (TypeError $e) { $out = $e->getMessage(); }
    echo $label, ' => ', $out, "\n";
};
$cmpValues = ['1' => 1, '1.5' => 1.5, 'true' => true, 'false' => false, 'null' => null,
    '[]' => [], 'STDIN' => STDIN, "''" => '', "'CmpNope'" => 'CmpNope', "'1'" => '1',
    "'CmpLeaf'" => 'CmpLeaf', "'\\CmpLeaf'" => '\CmpLeaf', 'new CmpLeaf' => new CmpLeaf];
foreach ($cmpValues as $label => $v) {
    $cmpShow("method_exists($label, 'm')", fn() => method_exists($v, 'm'));
    $cmpShow("property_exists($label, 'p')", fn() => property_exists($v, 'p'));
    $cmpShow("get_parent_class($label)", fn() => get_parent_class($v));
}
// The argument #2 screen still runs first, as php's ZPP does.
$cmpShow("property_exists(1, [])", fn() => property_exists(1, []));
$cmpShow("method_exists(1, [])", fn() => method_exists(1, []));
// Reached by name rather than by a direct call.
$cmpShow("call_user_func('property_exists', 1, 'p')", fn() => call_user_func('property_exists', 1, 'p'));
$cmpFn = 'get_parent_class';
$cmpShow("\$cmpFn(1)", fn() => $cmpFn(1));
// get_parent_class() still declares object|string to Reflection.
echo (new ReflectionFunction('get_parent_class'))->getParameters()[0]->getType(), "\n";
--EXPECT--
method_exists(1, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, int given
property_exists(1, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, int given
get_parent_class(1) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, int given
method_exists(1.5, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, float given
property_exists(1.5, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, float given
get_parent_class(1.5) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, float given
method_exists(true, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, true given
property_exists(true, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, true given
get_parent_class(true) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, true given
method_exists(false, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, false given
property_exists(false, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, false given
get_parent_class(false) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, false given
method_exists(null, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, null given
property_exists(null, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, null given
get_parent_class(null) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, null given
method_exists([], 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, array given
property_exists([], 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, array given
get_parent_class([]) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, array given
method_exists(STDIN, 'm') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, resource given
property_exists(STDIN, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, resource given
get_parent_class(STDIN) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, resource given
method_exists('', 'm') => false
property_exists('', 'p') => false
get_parent_class('') => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, string given
method_exists('CmpNope', 'm') => false
property_exists('CmpNope', 'p') => false
get_parent_class('CmpNope') => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, string given
method_exists('1', 'm') => false
property_exists('1', 'p') => false
get_parent_class('1') => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, string given
method_exists('CmpLeaf', 'm') => true
property_exists('CmpLeaf', 'p') => true
get_parent_class('CmpLeaf') => 'CmpBase'
method_exists('\CmpLeaf', 'm') => true
property_exists('\CmpLeaf', 'p') => true
get_parent_class('\CmpLeaf') => 'CmpBase'
method_exists(new CmpLeaf, 'm') => true
property_exists(new CmpLeaf, 'p') => true
get_parent_class(new CmpLeaf) => 'CmpBase'
property_exists(1, []) => property_exists(): Argument #2 ($property) must be of type string, array given
method_exists(1, []) => method_exists(): Argument #2 ($method) must be of type string, array given
call_user_func('property_exists', 1, 'p') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, int given
$cmpFn(1) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, int given
object|string
