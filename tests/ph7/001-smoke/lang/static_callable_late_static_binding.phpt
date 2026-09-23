--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static callable keeps the class it was called through (late static binding)
--FILE--
<?php
class LsbCallBase {
    public static function who() { return static::class; }
    public static function make() { return new static(); }
    public static function __callStatic($n, $a) { return "cs:$n@" . static::class; }
}
class LsbCallKid extends LsbCallBase {}

/* The direct spelling has always been right; every callable spelling of the same call
 * must answer with it. */
echo "direct:", LsbCallKid::who(), "\n";
echo "cuf-array:", call_user_func(['LsbCallKid', 'who']), "\n";
echo "cuf-string:", call_user_func('LsbCallKid::who'), "\n";
echo "cufa:", call_user_func_array(['LsbCallKid', 'who'], []), "\n";
$cbArray = ['LsbCallKid', 'who'];
echo "value-array:", $cbArray(), "\n";
$cbString = 'LsbCallKid::who';
echo "value-string:", $cbString(), "\n";
echo "array_map:", implode(",", array_map(['LsbCallKid', 'who'], [1])), "\n";
echo "fcc:", (LsbCallKid::who(...))(), "\n";
echo "fromCallable-array:", (Closure::fromCallable(['LsbCallKid', 'who']))(), "\n";
echo "fromCallable-string:", (Closure::fromCallable('LsbCallKid::who'))(), "\n";

/* `new static` is the same question asked for an OBJECT: a base factory reached through
 * the child must build the child. */
echo "new-static-direct:", get_class(LsbCallKid::make()), "\n";
echo "new-static-cuf:", get_class(call_user_func(['LsbCallKid', 'make'])), "\n";
echo "new-static-fcc:", get_class((LsbCallKid::make(...))()), "\n";

/* __callStatic is dispatched by the ENGINE, and php still reports the NAMED class. */
echo "magic-direct:", LsbCallKid::nope(), "\n";
echo "magic-cuf:", call_user_func(['LsbCallKid', 'nope']), "\n";
echo "magic-fcc:", (LsbCallKid::nope(...))(), "\n";

/* An instance callable was already right (the object carries the class) — keep it pinned. */
$o = new LsbCallKid();
echo "instance-cuf:", call_user_func([$o, 'who']), "\n";
echo "self-not-static:", LsbCallBase::who(), "\n";
?>
--EXPECT--
direct:LsbCallKid
cuf-array:LsbCallKid
cuf-string:LsbCallKid
cufa:LsbCallKid
value-array:LsbCallKid
value-string:LsbCallKid
array_map:LsbCallKid
fcc:LsbCallKid
fromCallable-array:LsbCallKid
fromCallable-string:LsbCallKid
new-static-direct:LsbCallKid
new-static-cuf:LsbCallKid
new-static-fcc:LsbCallKid
magic-direct:cs:nope@LsbCallKid
magic-cuf:cs:nope@LsbCallKid
magic-fcc:cs:nope@LsbCallKid
instance-cuf:LsbCallKid
self-not-static:LsbCallBase
--CLEAN--
<?php
