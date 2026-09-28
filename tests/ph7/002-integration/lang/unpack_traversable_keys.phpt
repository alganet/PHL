--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unpacking a Traversable reads its KEYS: php's int|string screen and named arguments
--DESCRIPTION--
An ARRAY source can only hand `...` the two key types an array holds, so both unpack sites had
simply thrown the iterator's key away. An ITERATOR can answer key() with anything, and php
screens it: `Keys must be of type int|string during array unpacking` for `[...$it]` and
`... during argument unpacking` for `f(...$it)`, an Error raised at the offending element — and
php refuses a float (a WHOLE one included), a bool, a null and a resource there, which is NOT
the array-offset rule set (that one folds all four).

What survives follows php's ordinary 8.1 rules, the ones an array source already got: a key that
stays a STRING is kept, a key that FOLDS to an integer — a canonical numeric string like "7"
among them — is renumbered. On the ARGUMENT path the kept string key is what makes the element a
NAMED argument; before this it was passed POSITIONALLY, so `h(...$gen)` yielding `'z' => 9` filled
$x with 9 instead of $z, silently. The binder's own refusals come with it (unknown name, duplicate
name, positional after named), and php words the last one with a trailing ` during unpacking`
that the same rule broken by call_user_func_array() does not get.
--FILE--
<?php
function t($label, $fn) {
    echo "== $label\n";
    try { var_export($fn()); echo "\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
function gen($pairs) { return (function () use ($pairs) { foreach ($pairs as [$k, $v]) yield $k => $v; })(); }
function h($x = 1, $y = 2, $z = 3) { return "$x/$y/$z"; }
function v(...$a) { return $a; }

$f = fopen('php://memory', 'r');
$types = [
    'array'       => [1],
    'object'      => new stdClass(),
    'lossy float' => 1.9,
    'whole float' => 5.0,
    'bool'        => true,
    'null'        => null,
    'resource'    => $f,
];

echo "-> [...\$it] refuses every key an array cannot hold\n";
foreach ($types as $label => $k) {
    t($label, fn() => [...gen([[$k, 'v']])]);
}

echo "-> f(...\$it) refuses the same set, worded for arguments\n";
foreach ($types as $label => $k) {
    t($label, fn() => v(...gen([[$k, 'v']])));
}

echo "-> the two php DOES hold\n";
t('int key renumbered',        fn() => [...gen([[5, 'a'], [9, 'b']])]);
t('string key kept',           fn() => [...gen([['b', 'a'], ['c', 'd']])]);
t('numeric string is an int',  fn() => [...gen([['7', 'a'], ['07', 'b'], ['-3', 'c']])]);
t('later string key wins',     fn() => [...gen([['a', 1], ['a', 2]])]);

echo "-> a string key is a NAMED argument\n";
t('named binds by name',       fn() => h(...gen([['z', 9]])));
t('int key stays positional',  fn() => h(...gen([[7, 'a']])));
t('numeric string positional', fn() => h(...gen([['7', 'a']])));
t('non-canonical is a name',   fn() => v(...gen([['07', 'a']])));
t('unknown name',              fn() => h(...gen([['q', 1]])));
t('duplicate name',            fn() => h(...gen([['x', 1], ['x', 2]])));
t('duplicate across sources',  fn() => h(...gen([['x', 1]]), ...gen([['x', 2]])));
t('positional after named',    fn() => h(...gen([['y', 9], [0, 'v']])));
t('named after positional',    fn() => h(...gen([[0, 'v'], ['y', 9]])));

echo "-> call_user_func_array() breaks the same rule with php's OTHER sentence\n";
t('cufa positional after named', fn() => call_user_func_array('h', ['y' => 9, 'v']));

echo "-> an ARRAY source is unchanged\n";
t('array named',   fn() => h(...['z' => 9]));
t('array mixed',   fn() => [1, ...['a' => 1, 3 => 2], 'z']);
?>
--EXPECT--
-> [...$it] refuses every key an array cannot hold
== array
Error: Keys must be of type int|string during array unpacking
== object
Error: Keys must be of type int|string during array unpacking
== lossy float
Error: Keys must be of type int|string during array unpacking
== whole float
Error: Keys must be of type int|string during array unpacking
== bool
Error: Keys must be of type int|string during array unpacking
== null
Error: Keys must be of type int|string during array unpacking
== resource
Error: Keys must be of type int|string during array unpacking
-> f(...$it) refuses the same set, worded for arguments
== array
Error: Keys must be of type int|string during argument unpacking
== object
Error: Keys must be of type int|string during argument unpacking
== lossy float
Error: Keys must be of type int|string during argument unpacking
== whole float
Error: Keys must be of type int|string during argument unpacking
== bool
Error: Keys must be of type int|string during argument unpacking
== null
Error: Keys must be of type int|string during argument unpacking
== resource
Error: Keys must be of type int|string during argument unpacking
-> the two php DOES hold
== int key renumbered
array (
  0 => 'a',
  1 => 'b',
)
== string key kept
array (
  'b' => 'a',
  'c' => 'd',
)
== numeric string is an int
array (
  0 => 'a',
  '07' => 'b',
  1 => 'c',
)
== later string key wins
array (
  'a' => 2,
)
-> a string key is a NAMED argument
== named binds by name
'1/2/9'
== int key stays positional
'a/2/3'
== numeric string positional
'a/2/3'
== non-canonical is a name
array (
  '07' => 'a',
)
== unknown name
Error: Unknown named parameter $q
== duplicate name
Error: Named parameter $x overwrites previous argument
== duplicate across sources
Error: Named parameter $x overwrites previous argument
== positional after named
Error: Cannot use positional argument after named argument during unpacking
== named after positional
'v/9/3'
-> call_user_func_array() breaks the same rule with php's OTHER sentence
== cufa positional after named
Error: Cannot use positional argument after named argument
-> an ARRAY source is unchanged
== array named
'1/2/9'
== array mixed
array (
  0 => 1,
  'a' => 1,
  1 => 2,
  2 => 'z',
)
