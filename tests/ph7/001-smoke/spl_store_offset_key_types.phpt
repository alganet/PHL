--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An ArrayObject offset that is not a key at all is refused, not folded
--DESCRIPTION--
An OBJECT and an ARRAY have no array key, and php refuses both on every offset
an ArrayObject/ArrayIterator has — while this engine folded them to the STRINGS
"Object" and "Array", so `$ao[$obj] = 1` wrote under a key nothing could ask for
again and `$ao[$obj]` warned about a key the caller never wrote. The wording is
the engine's own three-way split: a read or a write names the RECEIVER's class
(a subclass included), isset/empty names no class at all, and unset says "Cannot
unset". SplFixedArray refused both already but named the word "object" where php
names the CLASS. The keys that DO fold — null, bool, float, numeric string — are
untouched, and so is the writable-slot fast path a nested write goes through.
--FILE--
<?php
function sokShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
class SokArrayObject extends ArrayObject {}
foreach (['ArrayObject', 'ArrayIterator', 'SokArrayObject'] as $sokCls) {
    foreach (['object' => new stdClass, 'array' => [1]] as $sokWhat => $sokKey) {
        sokShow("$sokCls set $sokWhat", function () use ($sokCls, $sokKey) {
            $a = new $sokCls(); $a[$sokKey] = 1; return 'written';
        });
        sokShow("$sokCls get $sokWhat", fn() => (new $sokCls(['x' => 1]))[$sokKey]);
        sokShow("$sokCls isset $sokWhat", fn() => isset((new $sokCls(['x' => 1]))[$sokKey]));
        sokShow("$sokCls empty $sokWhat", fn() => empty((new $sokCls(['x' => 1]))[$sokKey]));
        sokShow("$sokCls unset $sokWhat", function () use ($sokCls, $sokKey) {
            $a = new $sokCls(['x' => 1]); unset($a[$sokKey]); return 'unset';
        });
        sokShow("$sokCls offsetGet $sokWhat",
            fn() => (new $sokCls(['x' => 1]))->offsetGet($sokKey));
        sokShow("$sokCls offsetExists $sokWhat",
            fn() => (new $sokCls(['x' => 1]))->offsetExists($sokKey));
    }
}
sokShow('SplFixedArray object', function () { $f = new SplFixedArray(2); $f[new stdClass] = 1; });
sokShow('SplFixedArray array', function () { $f = new SplFixedArray(2); $f[[1]] = 1; });
sokShow('SplObjectStorage array', fn() => isset((new SplObjectStorage)[[1]]));

/* Everything that DOES fold to a key still folds to it. */
sokShow('the keys that fold', function () {
    $a = new ArrayObject();
    $a[] = 'append';
    $a[3] = 'int';
    $a[true] = 'bool';
    $a[2.0] = 'float';   /* integral: a fractional one is php's own deprecation */
    $a['7'] = 'numeric string';
    $a['k'] = 'string';
    return $a->getArrayCopy();
});
sokShow('the writable slot still lands', function () {
    $a = new ArrayObject(['a' => ['b' => 1]]);
    $a['a']['b'] = 2;
    $a['new']['k'] = 5;
    return $a->getArrayCopy();
});
sokShow('an object key is still a key where php has one', function () {
    $m = new WeakMap; $o = new stdClass; $m[$o] = 5;
    $s = new SplObjectStorage; $s[$o] = 6;
    return [$m[$o], $s[$o], count($m), count($s)];
});
sokShow('the walk survives a refusal', function () {
    $a = new ArrayIterator(['x' => 1, 'y' => 2]);
    try { $a[new stdClass]; } catch (Throwable $e) {}
    $out = [];
    foreach ($a as $k => $v) { $out[] = "$k=$v"; }
    return $out;
});
--EXPECT--
ArrayObject set object => TypeError: Cannot access offset of type stdClass on ArrayObject
ArrayObject get object => TypeError: Cannot access offset of type stdClass on ArrayObject
ArrayObject isset object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayObject empty object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayObject unset object => TypeError: Cannot unset offset of type stdClass on ArrayObject
ArrayObject offsetGet object => TypeError: Cannot access offset of type stdClass on ArrayObject
ArrayObject offsetExists object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayObject set array => TypeError: Cannot access offset of type array on ArrayObject
ArrayObject get array => TypeError: Cannot access offset of type array on ArrayObject
ArrayObject isset array => TypeError: Cannot access offset of type array in isset or empty
ArrayObject empty array => TypeError: Cannot access offset of type array in isset or empty
ArrayObject unset array => TypeError: Cannot unset offset of type array on ArrayObject
ArrayObject offsetGet array => TypeError: Cannot access offset of type array on ArrayObject
ArrayObject offsetExists array => TypeError: Cannot access offset of type array in isset or empty
ArrayIterator set object => TypeError: Cannot access offset of type stdClass on ArrayIterator
ArrayIterator get object => TypeError: Cannot access offset of type stdClass on ArrayIterator
ArrayIterator isset object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayIterator empty object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayIterator unset object => TypeError: Cannot unset offset of type stdClass on ArrayIterator
ArrayIterator offsetGet object => TypeError: Cannot access offset of type stdClass on ArrayIterator
ArrayIterator offsetExists object => TypeError: Cannot access offset of type stdClass in isset or empty
ArrayIterator set array => TypeError: Cannot access offset of type array on ArrayIterator
ArrayIterator get array => TypeError: Cannot access offset of type array on ArrayIterator
ArrayIterator isset array => TypeError: Cannot access offset of type array in isset or empty
ArrayIterator empty array => TypeError: Cannot access offset of type array in isset or empty
ArrayIterator unset array => TypeError: Cannot unset offset of type array on ArrayIterator
ArrayIterator offsetGet array => TypeError: Cannot access offset of type array on ArrayIterator
ArrayIterator offsetExists array => TypeError: Cannot access offset of type array in isset or empty
SokArrayObject set object => TypeError: Cannot access offset of type stdClass on SokArrayObject
SokArrayObject get object => TypeError: Cannot access offset of type stdClass on SokArrayObject
SokArrayObject isset object => TypeError: Cannot access offset of type stdClass in isset or empty
SokArrayObject empty object => TypeError: Cannot access offset of type stdClass in isset or empty
SokArrayObject unset object => TypeError: Cannot unset offset of type stdClass on SokArrayObject
SokArrayObject offsetGet object => TypeError: Cannot access offset of type stdClass on SokArrayObject
SokArrayObject offsetExists object => TypeError: Cannot access offset of type stdClass in isset or empty
SokArrayObject set array => TypeError: Cannot access offset of type array on SokArrayObject
SokArrayObject get array => TypeError: Cannot access offset of type array on SokArrayObject
SokArrayObject isset array => TypeError: Cannot access offset of type array in isset or empty
SokArrayObject empty array => TypeError: Cannot access offset of type array in isset or empty
SokArrayObject unset array => TypeError: Cannot unset offset of type array on SokArrayObject
SokArrayObject offsetGet array => TypeError: Cannot access offset of type array on SokArrayObject
SokArrayObject offsetExists array => TypeError: Cannot access offset of type array in isset or empty
SplFixedArray object => TypeError: Cannot access offset of type stdClass on SplFixedArray
SplFixedArray array => TypeError: Cannot access offset of type array on SplFixedArray
SplObjectStorage array => TypeError: SplObjectStorage::offsetExists(): Argument #1 ($object) must be of type object, array given
the keys that fold => array (  0 => 'append',  3 => 'int',  1 => 'bool',  2 => 'float',  7 => 'numeric string',  'k' => 'string',)
the writable slot still lands => array (  'a' =>   array (    'b' => 2,  ),  'new' =>   array (    'k' => 5,  ),)
an object key is still a key where php has one => array (  0 => 5,  1 => 6,  2 => 1,  3 => 1,)
the walk survives a refusal => array (  0 => 'x=1',  1 => 'y=2',)
