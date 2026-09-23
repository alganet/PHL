--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize_callback_func: one chance to define the class, php's two failure shapes
--DESCRIPTION--
When a payload names a class that stays unknown after autoload, php consults the
unserialize_callback_func ini before falling back to __PHP_Incomplete_Class: the
named function is called with the class name and gets one chance to define it.
A callback that defines nothing takes php's warning (`Function f() hasn't
defined the class it was called for`) and the carrier is built anyway; an ini
naming a function that does not exist is a catchable Error naming the callback;
and a class the allowed_classes OPTION refused never consults the callback at
all — the refusal comes first, autoloader included. The ini key did not exist in
PHL (ini_set() refused it), so the whole hook was unreachable.
--FILE--
<?php
set_error_handler(function ($n, $s) { echo '  [', $n, '] ', $s, "\n"; return true; });

echo "-- the callback gets one chance to define the class\n";
function uicbDefine($name) { echo "CB:$name\n"; eval("class $name { public \$x; }"); }
ini_set('unserialize_callback_func', 'uicbDefine');
$uicbR = unserialize('O:8:"UicbWdef":1:{s:1:"x";i:5;}');
echo get_class($uicbR), "\n";

echo "-- a callback that defines nothing: php's warning, then the carrier\n";
function uicbNothing($name) { echo "CB2:$name\n"; }
ini_set('unserialize_callback_func', 'uicbNothing');
echo get_class(unserialize('O:10:"UicbWnodef":0:{}')), "\n";

echo "-- a callback that does not exist: a catchable Error\n";
ini_set('unserialize_callback_func', 'uicb_no_such_fn');
try { unserialize('O:8:"UicbQqqq":0:{}'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- never consulted for a class the OPTION refused\n";
ini_set('unserialize_callback_func', 'uicbNothing');
echo get_class(unserialize('O:8:"UicbAaaa":0:{}', ['allowed_classes' => false])), "\n";
ini_set('unserialize_callback_func', '');
--EXPECT--
-- the callback gets one chance to define the class
CB:UicbWdef
UicbWdef
-- a callback that defines nothing: php's warning, then the carrier
CB2:UicbWnodef
  [2] unserialize(): Function uicbNothing() hasn't defined the class it was called for
__PHP_Incomplete_Class
-- a callback that does not exist: a catchable Error
Error: Invalid callback uicb_no_such_fn, function "uicb_no_such_fn" not found or invalid function name
-- never consulted for a class the OPTION refused
__PHP_Incomplete_Class
