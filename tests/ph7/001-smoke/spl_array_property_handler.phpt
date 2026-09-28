--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ARRAY_AS_PROPS is a property HANDLER, and the FLAG is the whole handler
--DESCRIPTION--
php's ArrayObject / ArrayIterator declare no __get/__set/__isset/__unset: the property
surface is a C handler whose `spl_array_get_hash_table` answers the storage only when
SPL_ARRAY_ARRAY_AS_PROPS is set, and stands down entirely when it is not. PHL reached the
storage through the magic four -- methods php does not have -- and those bodies could not
stand down: with the flag OFF they answered a storage key null and swallowed the write in
silence where php warns and creates a property BESIDE the storage, and with it ON they
answered a missing key in silence where php reports the missing ARRAY KEY.
property_exists() never consulted the handler at all, ArrayIterator had no property
surface whatever its flag said, and __debugInfo -- which php declares on both so a program
can read the array var_dump() prints -- was missing. A write-context fetch is php's
get_property_ptr_ptr and hands back the store's OWN element, so `$o->list[] = 1` and
`sort($o->nums)` land in the storage rather than in a temporary.
--FILE--
<?php
$splPropMagic = [];
foreach (['ArrayObject', 'ArrayIterator', 'RecursiveArrayIterator'] as $splPropCls) {
    foreach (get_class_methods($splPropCls) as $splPropM) {
        if (in_array(strtolower($splPropM), ['__get', '__set', '__isset', '__unset'], true)) {
            $splPropMagic[] = $splPropCls . '::' . $splPropM;
        }
    }
    echo $splPropCls, ' debugInfo=', var_export(method_exists($splPropCls, '__debugInfo'), true), "\n";
}
echo 'magic accessors: ', $splPropMagic ? implode(',', $splPropMagic) : '(none)', "\n";

// Without the flag the handler stands down: php's standard property path answers.
$splPropStd = new ArrayObject(['x' => 1]);
echo 'std read=';
var_export(@$splPropStd->x);
echo ' std isset=', var_export(isset($splPropStd->x), true),
     ' std exists=', var_export(property_exists($splPropStd, 'x'), true), "\n";

// With it, every name is the storage's -- and a missing one is the missing ARRAY KEY.
$splPropAs = new ArrayObject(['x' => 1, 'nul' => null, 'zero' => 0],
    ArrayObject::ARRAY_AS_PROPS);
echo 'read=', var_export($splPropAs->x, true),
     ' coalesce absent=', var_export($splPropAs->absent ?? 'D', true), "\n";
echo 'read absent=';
var_export(@$splPropAs->absent);
echo "\n";
foreach (['x', 'nul', 'zero', 'absent'] as $splPropK) {
    echo '  ', $splPropK,
         ': isset=', var_export(isset($splPropAs->$splPropK), true),
         ' empty=', var_export(empty($splPropAs->$splPropK), true),
         ' exists=', var_export(property_exists($splPropAs, $splPropK), true), "\n";
}
// empty() asks the truth of a COPY: the element must still be the int it holds.
var_dump($splPropAs->x);

$splPropAs->added = 'v';
$splPropAs->x .= '!';
unset($splPropAs->nul);
echo 'after writes=', json_encode($splPropAs->getArrayCopy()), "\n";

// A write-context fetch is the store's own element, created when it is missing.
$splPropW = new ArrayObject([], ArrayObject::ARRAY_AS_PROPS);
$splPropW->list[] = 'A';
$splPropW->list[] = 'B';
$splPropW->nums = [3, 1, 2];
sort($splPropW->nums);
$splPropRef = &$splPropW->nums;
$splPropRef[] = 'through the reference';
echo 'write-through=', json_encode($splPropW->getArrayCopy()), "\n";

// A REAL property of a subclass is in the handler's way, exactly as php's is.
class SplPropSub extends ArrayObject { public $own = 'slot'; }
$splPropSub = new SplPropSub(['own' => 'storage', 'k' => 'v'], ArrayObject::ARRAY_AS_PROPS);
$splPropSub->own = 'written';
$splPropSub->k = 'also written';
echo 'own=', $splPropSub->own, ' storage=', json_encode($splPropSub->getArrayCopy()), "\n";

// ArrayIterator carries the same handler, and __debugInfo names the class that
// DECLARED the storage under php's mangled key.
$splPropIt = new ArrayIterator(['k' => 'v'], ArrayIterator::ARRAY_AS_PROPS);
$splPropIt->n = 1;
echo 'iterator=', json_encode($splPropIt->getArrayCopy()),
     ' read=', var_export($splPropIt->k, true), "\n";
echo 'debug keys=', json_encode(array_map(
    fn($k) => str_replace("\0", '@', $k), array_keys($splPropIt->__debugInfo()))), "\n";
--EXPECT--
ArrayObject debugInfo=true
ArrayIterator debugInfo=true
RecursiveArrayIterator debugInfo=true
magic accessors: (none)
std read=NULL std isset=false std exists=false
read=1 coalesce absent='D'
read absent=NULL
  x: isset=true empty=false exists=true
  nul: isset=false empty=true exists=true
  zero: isset=true empty=true exists=true
  absent: isset=false empty=true exists=false
int(1)
after writes={"x":"1!","zero":0,"added":"v"}
write-through={"list":["A","B"],"nums":[1,2,3,"through the reference"]}
own=written storage={"own":"storage","k":"also written"}
iterator={"k":"v","n":1} read='v'
debug keys=["@ArrayIterator@storage"]
