--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A key that arrives as a VALUE takes php's array-offset rules, not the string cast
--DESCRIPTION--
Three doors key an array by a value the script produced rather than by a subscript:
iterator_to_array() stores a Traversable's own key(), CachingIterator's FULL_CACHE caches the
pair it just fetched, and array_column() keys its result by the $index_key COLUMN of each row.
php runs all three through the same machinery `$a[$k] = v` uses, so an object (a Stringable one
included) or an array is a TypeError, a RESOURCE warns and becomes its integer id, and a null
deprecates and lands on the "" key. PHL had let the ordinary string CAST decide instead, so
those keys came out as the literal "Array", "Object" and "Resource id #N" -- strings php never
writes, and for the object one a key php refuses to write at all. All three share
PH7_VmArrayKeyArg now, the rail array_key_exists() already used.

The error handler is used so the assertions match the message BODY on both engines regardless
of the log-copy prefix. The LOSSY-FLOAT key is the one divergence and lives in its own
twin pair, traversable_key_lossy_float{,_zend}.phpt.
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

class KeyBare {}
class KeyStr { public function __toString() { return "x"; } }

function gen($k) { return (function () use ($k) { yield $k => 'v'; })(); }

function t($label, $fn) {
    echo "== $label\n";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "-> iterator_to_array(): types php REJECTS\n";
t('object',            fn() => iterator_to_array(gen(new KeyBare())));
t('stringable object', fn() => iterator_to_array(gen(new KeyStr())));
t('array',             fn() => iterator_to_array(gen([1])));

echo "-> iterator_to_array(): types php FOLDS\n";
t('null -> ""',      fn() => iterator_to_array(gen(null)));
t('true -> 1',       fn() => iterator_to_array(gen(true)));
t('whole float 5.0', fn() => iterator_to_array(gen(5.0)));
t('numeric string',  fn() => iterator_to_array(gen("7")));
t('plain string',    fn() => iterator_to_array(gen("k")));

echo "-> iterator_to_array(): a RESOURCE key warns and becomes its id\n";
$f = fopen('php://memory', 'r');
t('resource', function () use ($f) { return array_keys(iterator_to_array(gen($f))) === [(int)$f]; });
echo "the iterator's own key is untouched: ";
foreach (gen($f) as $k => $_) { var_dump(is_resource($k)); }

echo "-> iterator_to_array(): \$preserve_keys=false never asks\n";
t('array key, renumbered', fn() => iterator_to_array(gen([1]), false));

echo "-> iterator_count() never asks either\n";
t('array key', fn() => iterator_count(gen([1])));

echo "-> array_column(): the \$index_key COLUMN\n";
t('object',            fn() => array_column([['a' => 1, 'b' => new KeyBare()]], 'a', 'b'));
t('stringable object', fn() => array_column([['a' => 1, 'b' => new KeyStr()]], 'a', 'b'));
t('array',             fn() => array_column([['a' => 1, 'b' => [2]]], 'a', 'b'));
t('object ROW, array', fn() => array_column([(object)['a' => 1, 'b' => [2]]], 'a', 'b'));
t('null -> ""',        fn() => array_column([['a' => 1, 'b' => null]], 'a', 'b'));
t('true -> 1',         fn() => array_column([['a' => 1, 'b' => true]], 'a', 'b'));
t('whole float 5.0',   fn() => array_column([['a' => 1, 'b' => 5.0]], 'a', 'b'));
t('numeric string',    fn() => array_column([['a' => 1, 'b' => "7"]], 'a', 'b'));
t('resource',          function () use ($f) {
    return array_keys(array_column([['a' => 1, 'b' => $f]], 'a', 'b')) === [(int)$f];
});
echo "a row MISSING the index column is still appended: ";
var_dump(array_column([['a' => 1]], 'a', 'b'));

echo "-> CachingIterator FULL_CACHE\n";
function cached($k) {
    $c = new CachingIterator(gen($k), CachingIterator::FULL_CACHE);
    foreach ($c as $_) {}
    return $c->getCache();
}
t('object',      fn() => cached(new KeyBare()));
t('array',       fn() => cached([1]));
t('null -> ""',  fn() => cached(null));
t('resource',    function () use ($f) { return array_keys(cached($f)) === [(int)$f]; });
?>
--EXPECTF--
-> iterator_to_array(): types php REJECTS
== object
TypeError: Cannot access offset of type KeyBare on array
== stringable object
TypeError: Cannot access offset of type KeyStr on array
== array
TypeError: Cannot access offset of type array on array
-> iterator_to_array(): types php FOLDS
== null -> ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(1) {
  [""]=>
  string(1) "v"
}
== true -> 1
array(1) {
  [1]=>
  string(1) "v"
}
== whole float 5.0
array(1) {
  [5]=>
  string(1) "v"
}
== numeric string
array(1) {
  [7]=>
  string(1) "v"
}
== plain string
array(1) {
  ["k"]=>
  string(1) "v"
}
-> iterator_to_array(): a RESOURCE key warns and becomes its id
== resource
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(true)
the iterator's own key is untouched: bool(true)
-> iterator_to_array(): $preserve_keys=false never asks
== array key, renumbered
array(1) {
  [0]=>
  string(1) "v"
}
-> iterator_count() never asks either
== array key
int(1)
-> array_column(): the $index_key COLUMN
== object
TypeError: Cannot access offset of type KeyBare on array
== stringable object
TypeError: Cannot access offset of type KeyStr on array
== array
TypeError: Cannot access offset of type array on array
== object ROW, array
TypeError: Cannot access offset of type array on array
== null -> ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(1) {
  [""]=>
  int(1)
}
== true -> 1
array(1) {
  [1]=>
  int(1)
}
== whole float 5.0
array(1) {
  [5]=>
  int(1)
}
== numeric string
array(1) {
  [7]=>
  int(1)
}
== resource
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(true)
a row MISSING the index column is still appended: array(1) {
  [0]=>
  int(1)
}
-> CachingIterator FULL_CACHE
== object
TypeError: Cannot access offset of type KeyBare on array
== array
TypeError: Cannot access offset of type array on array
== null -> ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(1) {
  [""]=>
  string(1) "v"
}
== resource
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(true)
