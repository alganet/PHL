--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native ArrayAccess offset takes every array-offset rule, not just the two refusals
--DESCRIPTION--
php's ArrayObject / ArrayIterator / SplFixedArray hand their offset to the same machinery
`$a[$k]` uses, so all of the engine's answers belong to them and not only the object/array
refusals they already had. A RESOURCE offset warns and becomes its integer id -- the string cast
had been keying it under the literal "Resource id #N", a key php never writes and one only
`$ao[$res]` could read back -- and a NULL offset deprecates and reads the "" key, except in
offsetSet() where php's null key is the append form. SplFixedArray had refused a resource
outright, naming a type php accepts.

The engine's SPL fast path (PH7_SplDimElemSlot, which reads an element slot without calling
offsetGet at all) had to stand down for both of those, as it already did for null: it looks the
key up RAW, which is where the "Resource id #N" came from.

The LOSSY-FLOAT offset is the one divergence and lives in its own twin pair,
native_arrayaccess_lossy_float{,_zend}.phpt.
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
function t($label, $fn) {
    echo "== $label\n";
    try { var_export($fn()); echo "\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
$res = fopen('php://memory', 'r');
$id  = (int)$res;

class NaoSub extends ArrayObject {}

foreach (['ArrayObject', 'ArrayIterator', 'RecursiveArrayIterator', 'NaoSub'] as $cls) {
    echo "-> $cls\n";
    t('write under the id',  function () use ($cls, $res, $id) {
        $o = new $cls(); $o[$res] = 'v'; return array_keys($o->getArrayCopy()) === [$id];
    });
    t('read it back',        function () use ($cls, $res, $id) {
        $o = new $cls([$id => 'hit']); return $o[$res];
    });
    t('isset',               function () use ($cls, $res, $id) {
        $o = new $cls([$id => 'hit']); return isset($o[$res]);
    });
    t('unset',               function () use ($cls, $res, $id) {
        $o = new $cls([$id => 'hit']); unset($o[$res]); return $o->getArrayCopy();
    });
    t('offsetGet by hand',   function () use ($cls, $res, $id) {
        $o = new $cls([$id => 'hit']); return $o->offsetGet($res);
    });
    t('the key is untouched', function () use ($cls, $res) {
        $o = new $cls(); $o[$res] = 'v'; return is_resource($res);
    });
    t('null reads ""',       function () use ($cls) { $o = new $cls(['' => 'e']); return $o[null]; });
    t('null isset',          function () use ($cls) { $o = new $cls(['' => 'e']); return isset($o[null]); });
    t('null unset',          function () use ($cls) {
        $o = new $cls(['' => 'e', 1 => 'x']); unset($o[null]); return $o->getArrayCopy();
    });
    t('null WRITE appends',  function () use ($cls) { $o = new $cls(); $o[null] = 'v'; return $o->getArrayCopy(); });
    t('object refused',      function () use ($cls) { $o = new $cls(); $o[new stdClass()] = 'v'; });
    t('array refused',       function () use ($cls) { $o = new $cls(); $o[[1]] = 'v'; });
    t('isset object',        function () use ($cls) { $o = new $cls(); return isset($o[new stdClass()]); });
    t('unset array',         function () use ($cls) { $o = new $cls(); unset($o[[1]]); });
}

echo "-> SplFixedArray\n";
t('write under the id', function () use ($res, $id) { $o = new SplFixedArray($id + 2); $o[$res] = 'v'; return $o[$id]; });
t('read it back',       function () use ($res, $id) { $o = new SplFixedArray($id + 2); $o[$id] = 'hit'; return $o[$res]; });
t('isset',              function () use ($res, $id) { $o = new SplFixedArray($id + 2); $o[$id] = 'hit'; return isset($o[$res]); });
t('out of range',       function () use ($res)      { $o = new SplFixedArray(1); $o[$res] = 'v'; });
t('bool',               function ()                 { $o = new SplFixedArray(3); $o[true] = 'v'; return $o->toArray(); });
t('numeric string',     function ()                 { $o = new SplFixedArray(3); $o["2"] = 'v'; return $o->toArray(); });
t('null refused',       function ()                 { $o = new SplFixedArray(3); $o[null] = 'v'; });
t('object refused',     function ()                 { $o = new SplFixedArray(3); $o[new stdClass()] = 'v'; });
?>
--EXPECTF--
-> ArrayObject
== write under the id
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== read it back
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== isset
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== unset
  [2] Resource ID#%d used as offset, casting to integer (%d)
array (
)
== offsetGet by hand
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== the key is untouched
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== null reads ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
'e'
== null isset
  [8192] Using null as an array offset is deprecated, use an empty string instead
true
== null unset
  [8192] Using null as an array offset is deprecated, use an empty string instead
array (
  1 => 'x',
)
== null WRITE appends
array (
  0 => 'v',
)
== object refused
TypeError: Cannot access offset of type stdClass on ArrayObject
== array refused
TypeError: Cannot access offset of type array on ArrayObject
== isset object
TypeError: Cannot access offset of type stdClass in isset or empty
== unset array
TypeError: Cannot unset offset of type array on ArrayObject
-> ArrayIterator
== write under the id
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== read it back
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== isset
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== unset
  [2] Resource ID#%d used as offset, casting to integer (%d)
array (
)
== offsetGet by hand
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== the key is untouched
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== null reads ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
'e'
== null isset
  [8192] Using null as an array offset is deprecated, use an empty string instead
true
== null unset
  [8192] Using null as an array offset is deprecated, use an empty string instead
array (
  1 => 'x',
)
== null WRITE appends
array (
  0 => 'v',
)
== object refused
TypeError: Cannot access offset of type stdClass on ArrayIterator
== array refused
TypeError: Cannot access offset of type array on ArrayIterator
== isset object
TypeError: Cannot access offset of type stdClass in isset or empty
== unset array
TypeError: Cannot unset offset of type array on ArrayIterator
-> RecursiveArrayIterator
== write under the id
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== read it back
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== isset
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== unset
  [2] Resource ID#%d used as offset, casting to integer (%d)
array (
)
== offsetGet by hand
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== the key is untouched
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== null reads ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
'e'
== null isset
  [8192] Using null as an array offset is deprecated, use an empty string instead
true
== null unset
  [8192] Using null as an array offset is deprecated, use an empty string instead
array (
  1 => 'x',
)
== null WRITE appends
array (
  0 => 'v',
)
== object refused
TypeError: Cannot access offset of type stdClass on RecursiveArrayIterator
== array refused
TypeError: Cannot access offset of type array on RecursiveArrayIterator
== isset object
TypeError: Cannot access offset of type stdClass in isset or empty
== unset array
TypeError: Cannot unset offset of type array on RecursiveArrayIterator
-> NaoSub
== write under the id
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== read it back
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== isset
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== unset
  [2] Resource ID#%d used as offset, casting to integer (%d)
array (
)
== offsetGet by hand
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== the key is untouched
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== null reads ""
  [8192] Using null as an array offset is deprecated, use an empty string instead
'e'
== null isset
  [8192] Using null as an array offset is deprecated, use an empty string instead
true
== null unset
  [8192] Using null as an array offset is deprecated, use an empty string instead
array (
  1 => 'x',
)
== null WRITE appends
array (
  0 => 'v',
)
== object refused
TypeError: Cannot access offset of type stdClass on NaoSub
== array refused
TypeError: Cannot access offset of type array on NaoSub
== isset object
TypeError: Cannot access offset of type stdClass in isset or empty
== unset array
TypeError: Cannot unset offset of type array on NaoSub
-> SplFixedArray
== write under the id
  [2] Resource ID#%d used as offset, casting to integer (%d)
'v'
== read it back
  [2] Resource ID#%d used as offset, casting to integer (%d)
'hit'
== isset
  [2] Resource ID#%d used as offset, casting to integer (%d)
true
== out of range
  [2] Resource ID#%d used as offset, casting to integer (%d)
OutOfBoundsException: Index invalid or out of range
== bool
array (
  0 => NULL,
  1 => 'v',
  2 => NULL,
)
== numeric string
array (
  0 => NULL,
  1 => NULL,
  2 => 'v',
)
== null refused
TypeError: Cannot access offset of type null on SplFixedArray
== object refused
TypeError: Cannot access offset of type stdClass on SplFixedArray
