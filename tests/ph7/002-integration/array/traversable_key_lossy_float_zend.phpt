--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: a LOSSY float key from a VALUE deprecates and truncates (php half)
--DESCRIPTION--
The php half of traversable_key_lossy_float.phpt: php emits `Implicit conversion from float 5.7
to int loses precision` (E_DEPRECATED) and keys the array by the truncated int. PHL has no
engine deprecation sites and rejects the lossy float instead (the scope policy).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
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
  [8192] Implicit conversion from float 5.7 to int loses precision
array(1) {
  [5]=>
  string(1) "v"
}
== array_column
  [8192] Implicit conversion from float -5.7 to int loses precision
array(1) {
  [-5]=>
  int(1)
}
== CachingIterator
  [8192] Implicit conversion from float 5.7 to int loses precision
array(1) {
  [5]=>
  string(1) "v"
}
== whole float stays a key
array(1) {
  [5]=>
  string(1) "v"
}
