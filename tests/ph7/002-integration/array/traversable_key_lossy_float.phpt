--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a LOSSY float key from a VALUE is PHL's TypeError (PHL half)
--DESCRIPTION--
php DEPRECATES a lossy float used as an array offset and truncates it, so a generator yielding
`5.7 => 'v'` collects into `[5 => 'v']` after `Implicit conversion from float 5.7 to int loses
precision`. PHL targets php's NON-deprecated surface (§10) and rejects the lossy float wherever
an offset is decided -- the subscript, array_key_exists(), and now the three doors that key by a
VALUE: iterator_to_array(), array_column()'s $index_key and CachingIterator's FULL_CACHE. A
WHOLE float is a key in both engines; only the lossy one diverges. php's half is the `_zend`
twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
function gen($k) { return (function () use ($k) { yield $k => 'v'; })(); }
function t($label, $fn) {
    echo "== $label\n";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
t('iterator_to_array', fn() => iterator_to_array(gen(5.7)));
t('array_column',      fn() => array_column([['a' => 1, 'b' => -5.7]], 'a', 'b'));
t('CachingIterator',   function () {
    $c = new CachingIterator(gen(5.7), CachingIterator::FULL_CACHE);
    foreach ($c as $_) {}
    return $c->getCache();
});
t('whole float stays a key', fn() => iterator_to_array(gen(5.0)));
?>
--EXPECT--
== iterator_to_array
TypeError: Cannot access offset of type float on array
== array_column
TypeError: Cannot access offset of type float on array
== CachingIterator
TypeError: Cannot access offset of type float on array
== whole float stays a key
array(1) {
  [5]=>
  string(1) "v"
}
