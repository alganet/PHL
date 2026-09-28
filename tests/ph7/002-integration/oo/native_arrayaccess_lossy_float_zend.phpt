--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a LOSSY float offset on a native ArrayAccess deprecates (php half)
--DESCRIPTION--
The php half of native_arrayaccess_lossy_float.phpt: php emits `Implicit conversion from float
1.9 to int loses precision` (E_DEPRECATED) and uses the truncated offset. PHL has no engine
deprecation sites and refuses the lossy float instead (§10).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
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
  [8192] Implicit conversion from float 1.9 to int loses precision
array (
  1 => 'v',
)
== read
  [8192] Implicit conversion from float 1.9 to int loses precision
'x'
== offsetGet
  [8192] Implicit conversion from float 1.9 to int loses precision
'x'
== empty reads
  [8192] Implicit conversion from float 1.9 to int loses precision
false
== iterator
  [8192] Implicit conversion from float 1.9 to int loses precision
array (
  1 => 'v',
)
== fixed
  [8192] Implicit conversion from float 1.9 to int loses precision
array (
  0 => NULL,
  1 => 'v',
  2 => NULL,
)
-> lenient exactly where the engine is lenient
== isset
  [8192] Implicit conversion from float 1.9 to int loses precision
true
== unset
  [8192] Implicit conversion from float 1.9 to int loses precision
array (
)
== array isset
  [8192] Implicit conversion from float 1.9 to int loses precision
true
== array unset
  [8192] Implicit conversion from float 1.9 to int loses precision
array (
)
== whole float is an offset
array (
  2 => 'v',
)
