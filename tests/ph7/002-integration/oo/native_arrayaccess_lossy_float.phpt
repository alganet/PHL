--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a LOSSY float offset on a native ArrayAccess is PHL's TypeError (PHL half)
--DESCRIPTION--
php DEPRECATES a lossy float used as an array offset and truncates it, ArrayObject and its
neighbours included. PHL targets php's NON-deprecated surface (§10) and refuses it wherever an
offset is decided -- the subscript, array_key_exists(), the value-keyed builtin doors, and now
the native ArrayAccess classes, which had been the one place that truncated it in SILENCE (a
different answer from the engine they live in AND from SplFixedArray next door, which already
refused it).

It refuses it exactly where the ENGINE does, which is a READ or a WRITE: `isset($a[1.9])` and
`unset($a[1.9])` truncate quietly on a plain array, so they do on the store too. `empty()` is the
one that looks like an exception and is not -- php's empty() on an ArrayAccess asks offsetExists
and then READS the value through offsetGet, and it is that read which refuses.

A WHOLE float is an offset in both engines; only the lossy one diverges. php's half is the
`_zend` twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
function t($label, $fn) {
    echo "== $label\n";
    try { var_export($fn()); echo "\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
t('write',      function () { $o = new ArrayObject();          $o[1.9] = 'v'; return $o->getArrayCopy(); });
t('read',       function () { $o = new ArrayObject([1 => 'x']); return $o[1.9]; });
t('offsetGet',  function () { $o = new ArrayObject([1 => 'x']); return $o->offsetGet(1.9); });
t('empty reads', function () { $o = new ArrayObject([1 => 'x']); return empty($o[1.9]); });
t('iterator',   function () { $o = new ArrayIterator();        $o[1.9] = 'v'; return $o->getArrayCopy(); });
t('fixed',      function () { $o = new SplFixedArray(3);       $o[1.9] = 'v'; return $o->toArray(); });
echo "-> lenient exactly where the engine is lenient\n";
t('isset',      function () { $o = new ArrayObject([1 => 'x']); return isset($o[1.9]); });
t('unset',      function () { $o = new ArrayObject([1 => 'x']); unset($o[1.9]); return $o->getArrayCopy(); });
t('array isset', function () { $a = [1 => 'x']; return isset($a[1.9]); });
t('array unset', function () { $a = [1 => 'x']; unset($a[1.9]); return $a; });
t('whole float is an offset', function () { $o = new ArrayObject(); $o[2.0] = 'v'; return $o->getArrayCopy(); });
?>
--EXPECT--
== write
TypeError: Cannot access offset of type float on ArrayObject
== read
TypeError: Cannot access offset of type float on ArrayObject
== offsetGet
TypeError: Cannot access offset of type float on ArrayObject
== empty reads
TypeError: Cannot access offset of type float on ArrayObject
== iterator
TypeError: Cannot access offset of type float on ArrayIterator
== fixed
TypeError: Cannot access offset of type float on SplFixedArray
-> lenient exactly where the engine is lenient
== isset
true
== unset
array (
)
== array isset
true
== array unset
array (
)
== whole float is an offset
array (
  2 => 'v',
)
