--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin's own TypeError names a bool by its value, as php does
--DESCRIPTION--
php's `, X given` tail is a VALUE name, not a type name: a bool is `true` or
`false` and an object is its class. The shared signature screen answered that
way already, but the ~90 screens builtins write for themselves went through the
plain type name, so `count(true)` said `bool given` where php says `true given`
— every array, hash, iterator, stream and DOM family among them. A class name
LONGER than the 64-byte buffer the tail is built in was cut down to fit, which
named a class that does not exist; it is answered from the class itself now.
--FILE--
<?php
function gvnTry($what, $fn) {
    try { $fn(); echo "$what => no error\n"; }
    catch (Throwable $e) { echo "$what => ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
/* A BOOL is named by its value, not by its type, at every screen a builtin
 * writes for itself. */
gvnTry('count(true)',                fn() => count(true));
gvnTry('count(false)',               fn() => count(false));
gvnTry('array_merge([], true)',      fn() => array_merge([], true));
gvnTry('array_diff([], false)',      fn() => array_diff([], false));
gvnTry('array_intersect([], true)',  fn() => array_intersect([], true));
gvnTry('array_replace([], true)',    fn() => array_replace([], true));
gvnTry('array_map(fn, [], true)',    fn() => array_map(fn($a, $b) => 1, [], true));
gvnTry('array_walk(true, fn)',       function () { $b = true; array_walk($b, fn() => 1); });
gvnTry('hash_equals(true, "x")',     fn() => hash_equals(true, 'x'));
gvnTry('hash_equals("x", false)',    fn() => hash_equals('x', false));
gvnTry('iterator_count(true)',       fn() => iterator_count(true));
gvnTry('iterator_to_array(false)',   fn() => iterator_to_array(false));
gvnTry('iterator_apply(true, fn)',   fn() => iterator_apply(true, fn() => true));
gvnTry('str_split(true, 1)',         fn() => str_split(true, 1));
/* An OBJECT is named by its CLASS, whole -- a name longer than the buffer the
 * message is built in is answered from the class itself. */
gvnTry('array_diff([], new stdClass)', fn() => array_diff([], new stdClass));
gvnTry('count(new stdClass)',          fn() => count(new stdClass));
$gvnLong = str_repeat('G', 100);
eval("class $gvnLong {}");
gvnTry('array_diff([], new G*100)',  fn() => array_diff([], new $GLOBALS['gvnLong']));
--EXPECT--
count(true) => TypeError: count(): Argument #1 ($value) must be of type Countable|array, true given
count(false) => TypeError: count(): Argument #1 ($value) must be of type Countable|array, false given
array_merge([], true) => TypeError: array_merge(): Argument #2 must be of type array, true given
array_diff([], false) => TypeError: array_diff(): Argument #2 must be of type array, false given
array_intersect([], true) => TypeError: array_intersect(): Argument #2 must be of type array, true given
array_replace([], true) => TypeError: array_replace(): Argument #2 must be of type array, true given
array_map(fn, [], true) => TypeError: array_map(): Argument #3 must be of type array, true given
array_walk(true, fn) => TypeError: array_walk(): Argument #1 ($array) must be of type array, true given
hash_equals(true, "x") => TypeError: hash_equals(): Argument #1 ($known_string) must be of type string, true given
hash_equals("x", false) => TypeError: hash_equals(): Argument #2 ($user_string) must be of type string, false given
iterator_count(true) => TypeError: iterator_count(): Argument #1 ($iterator) must be of type Traversable|array, true given
iterator_to_array(false) => TypeError: iterator_to_array(): Argument #1 ($iterator) must be of type Traversable|array, false given
iterator_apply(true, fn) => TypeError: iterator_apply(): Argument #1 ($iterator) must be of type Traversable, true given
str_split(true, 1) => no error
array_diff([], new stdClass) => TypeError: array_diff(): Argument #2 must be of type array, stdClass given
count(new stdClass) => TypeError: count(): Argument #1 ($value) must be of type Countable|array, stdClass given
array_diff([], new G*100) => TypeError: array_diff(): Argument #2 must be of type array, GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG given
