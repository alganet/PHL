--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parenthesised expression is a subscript base, whatever it holds
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

/* php's `dereferencable` list has `'(' expr ')'` in it, so whatever a group
 * evaluates to may be subscripted. The base test here was a whitelist of node
 * SHAPES, and no shape says "the user wrote parentheses" -- so every group
 * headed by an operator was `Invalid array name`, a compile fatal on source
 * php runs. `((array)$o)['k']` is the ordinary way to read one key out of an
 * object's array form. */
$o = (object)['k' => 7, 'n' => ['a' => 'deep']];
$s = 'abcd';
$ao = new ArrayObject([4, 5]);

/* a cast head -- the shape every probe had to be rewritten around */
var_dump(((array)$o)['k']);
var_dump(((array)$o)['n']['a']);
var_dump(((array)$o)['missing'] ?? 'default');
var_dump(isset(((array)$o)['k']), isset(((array)$o)['missing']));
var_dump(((string)$s)[1]);

/* every other operator head php accepts there */
var_dump((1 + 2)[0]);
var_dump((0 ?: [9])[0]);
var_dump((clone $ao)[1]);
var_dump((new ArrayObject([5]))[0]);
var_dump((fn() => [3])()[0]);

/* a plain TERM in parentheses, which the whitelist also missed for a number */
var_dump((1)[0]);
var_dump((null)[0]);
var_dump(('ab')[0]);
var_dump(([1, 2])[1]);

/* the group as a foreach subject */
foreach (((array)$o)['n'] as $k => $v) {
    var_dump([$k, $v]);
}
?>
--EXPECT--
int(7)
string(4) "deep"
string(7) "default"
bool(true)
bool(false)
string(1) "b"
Warning: Trying to access array offset on int
NULL
int(9)
int(5)
int(5)
int(3)
Warning: Trying to access array offset on int
NULL
Warning: Trying to access array offset on null
NULL
string(1) "a"
int(2)
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(4) "deep"
}
--CLEAN--
<?php
