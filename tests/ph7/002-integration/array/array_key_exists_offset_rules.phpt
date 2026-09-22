--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists()/key_exists() apply php's ARRAY-OFFSET rules to $key, not a string|int check
--DESCRIPTION--
php's array_key_exists() does not run a `string|int` ZPP row on its $key: it hands the value to
the same offset machinery `$a[$key]` uses, so the two agree on every type. PHL's builtin had its
own narrower check and therefore its own answers -- a TypeError for the case php ACCEPTS (null,
folded to the "" key) and a silent `false` for the ones php REJECTS or coerces (object, array,
resource). The object case was a wrong ANSWER, not just a missing diagnostic: a `__toString()`
object was stringified and could report TRUE for a key php refuses to look up at all. Both names
route through PH7_VmArrayKeyArg now, sharing the engine's VmOffsetTypeRejected /
VmOffsetResourceWarn. php words the illegal-type rejection differently in the alias
(`key_exists(): Argument #1 ($key) must be a valid array offset type`) than in
array_key_exists() (the engine's offset Error), and that asymmetry is reproduced.

The error handler is used so the assertions match the message BODY on both engines regardless
of the log-copy prefix (§6). The LOSSY-FLOAT key is the one divergence and lives in its own
twin pair, array_key_exists_lossy_float_key{,_zend}.phpt.
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

$a = ['x' => 1, 5 => 2, '' => 3, 0 => 4, 1 => 5];

function t($label, $fn) {
    echo "== $label\n";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

class AkeBare {}
class AkeStr { public function __toString() { return "x"; } }

echo "-> types php REJECTS\n";
t('object',            fn() => array_key_exists(new AkeBare(), $a));
t('stringable object', fn() => array_key_exists(new AkeStr(), $a));
t('array',             fn() => array_key_exists([1], $a));
t('object via alias',  fn() => key_exists(new AkeBare(), $a));
t('array via alias',   fn() => key_exists([1], $a));

echo "-> types php FOLDS\n";
t('null -> ""',        fn() => array_key_exists(null, $a));
t('true -> 1',         fn() => array_key_exists(true, $a));
t('false -> 0',        fn() => array_key_exists(false, $a));
t('whole float 5.0',   fn() => array_key_exists(5.0, $a));
t('numeric string',    fn() => array_key_exists("5", $a));
t('plain int',         fn() => array_key_exists(5, $a));
t('miss',              fn() => array_key_exists("nope", $a));

echo "-> a RESOURCE key warns and becomes its id\n";
$f = fopen('php://memory', 'r');
$byId = [(int)$f => 'hit'];
t('resource, present', function () use ($f, $byId) { return array_key_exists($f, $byId); });
t('resource, absent',  function () use ($f) { return array_key_exists($f, []); });
t('alias, present',    function () use ($f, $byId) { return key_exists($f, $byId); });
echo "the key argument itself is untouched: ";
var_dump(is_resource($f));

echo "-> \$array is still checked\n";
t('non-array',           fn() => array_key_exists('x', "s"));
t('non-array via alias', fn() => key_exists('x', "s"));
?>
--EXPECTF--
-> types php REJECTS
== object
TypeError: Cannot access offset of type AkeBare on array
== stringable object
TypeError: Cannot access offset of type AkeStr on array
== array
TypeError: Cannot access offset of type array on array
== object via alias
TypeError: key_exists(): Argument #1 ($key) must be a valid array offset type
== array via alias
TypeError: key_exists(): Argument #1 ($key) must be a valid array offset type
-> types php FOLDS
== null -> ""
  [8192] Using null as the key parameter for array_key_exists() is deprecated, use an empty string instead
bool(true)
== true -> 1
bool(true)
== false -> 0
bool(true)
== whole float 5.0
bool(true)
== numeric string
bool(true)
== plain int
bool(true)
== miss
bool(false)
-> a RESOURCE key warns and becomes its id
== resource, present
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(true)
== resource, absent
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(false)
== alias, present
  [2] Resource ID#%d used as offset, casting to integer (%d)
bool(true)
the key argument itself is untouched: bool(true)
-> $array is still checked
== non-array
TypeError: array_key_exists(): Argument #2 ($array) must be of type array, string given
== non-array via alias
TypeError: key_exists(): Argument #2 ($array) must be of type array, string given
