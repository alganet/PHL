--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An SPL container's __serialize() payload carries a SUBCLASS's own properties
--DESCRIPTION--
php's payload for SplObjectStorage, the SplDoublyLinkedList family, the heaps and
SplFixedArray is its own state plus a MEMBERS array, and that array is the instance's
real properties. For a bare container there are none, which is why every one of these
bodies shipped with a hardcoded empty array — and the moment a user subclasses one, its
declared properties belong in the payload. PHL dropped them, so serialize() round-tripped
a subclass back to its property DEFAULTS with nothing failing: `$s->p = 9` came back 1.
SplFixedArray is the odd shape and the reason the members walk cannot be assumed nested —
php puts the elements and the members in ONE array, told apart by their key (an int key is
an element, a string key is a property).
--FILE--
<?php
class SplSerKidStore extends SplObjectStorage { public $p = 1; }
class SplSerKidStack extends SplStack          { public $p = 1; }
class SplSerKidHeap  extends SplMinHeap        { public $p = 1; }
class SplSerKidFixed extends SplFixedArray     { public $p = 1; }
class SplSerKidQueue extends SplPriorityQueue  { public $p = 1; }

$obj = new stdClass;
$cases = [
    'SplObjectStorage' => function () use ($obj) { $s = new SplSerKidStore; $s[$obj] = 'i'; $s->p = 9; return $s; },
    'SplStack'         => function () { $s = new SplSerKidStack; $s->push(7); $s->p = 9; return $s; },
    'SplMinHeap'       => function () { $s = new SplSerKidHeap;  $s->insert(3); $s->p = 9; return $s; },
    'SplFixedArray'    => function () { $s = new SplSerKidFixed(2); $s[0] = 'a'; $s->p = 9; return $s; },
    'SplPriorityQueue' => function () { $s = new SplSerKidQueue; $s->insert('a', 1); $s->p = 9; return $s; },
];

echo "-- the payload carries the subclass's property\n";
foreach ($cases as $label => $make) {
    echo $label, ' => ', str_replace("\0", '^@', serialize($make())), "\n";
}

echo "-- and it survives the round trip\n";
foreach ($cases as $label => $make) {
    $back = unserialize(serialize($make()));
    printf("%s => %s p=%s count=%d\n", $label, get_class($back), var_export($back->p, true), count($back));
}

echo "-- a BARE container still has an empty members slot\n";
$bare = new SplObjectStorage;
$bare[$obj] = 'i';
echo 'SplObjectStorage => ', str_replace("\0", '^@', serialize($bare)), "\n";
echo 'SplStack => ', str_replace("\0", '^@', serialize(new SplStack)), "\n";
echo 'SplFixedArray => ', str_replace("\0", '^@', serialize(new SplFixedArray(2))), "\n";

echo "-- __serialize()/__unserialize() called directly agree with it\n";
$fx = new SplSerKidFixed(2);
$fx[1] = 'z';
$fx->p = 4;
echo 'SplFixedArray::__serialize => ', json_encode($fx->__serialize()), "\n";
$fresh = new SplSerKidFixed(0);
$fresh->__unserialize(['a', 'b', 'p' => 5]);
printf("__unserialize => count=%d [0]=%s p=%d\n", count($fresh), $fresh[0], $fresh->p);
--EXPECT--
-- the payload carries the subclass's property
SplObjectStorage => O:14:"SplSerKidStore":2:{i:0;a:2:{i:0;O:8:"stdClass":0:{}i:1;s:1:"i";}i:1;a:1:{s:1:"p";i:9;}}
SplStack => O:14:"SplSerKidStack":3:{i:0;i:6;i:1;a:1:{i:0;i:7;}i:2;a:1:{s:1:"p";i:9;}}
SplMinHeap => O:13:"SplSerKidHeap":2:{i:0;a:1:{s:1:"p";i:9;}i:1;a:2:{s:5:"flags";i:0;s:13:"heap_elements";a:1:{i:0;i:3;}}}
SplFixedArray => O:14:"SplSerKidFixed":3:{i:0;s:1:"a";i:1;N;s:1:"p";i:9;}
SplPriorityQueue => O:14:"SplSerKidQueue":2:{i:0;a:1:{s:1:"p";i:9;}i:1;a:2:{s:5:"flags";i:1;s:13:"heap_elements";a:1:{i:0;a:2:{s:4:"data";s:1:"a";s:8:"priority";i:1;}}}}
-- and it survives the round trip
SplObjectStorage => SplSerKidStore p=9 count=1
SplStack => SplSerKidStack p=9 count=1
SplMinHeap => SplSerKidHeap p=9 count=1
SplFixedArray => SplSerKidFixed p=9 count=2
SplPriorityQueue => SplSerKidQueue p=9 count=1
-- a BARE container still has an empty members slot
SplObjectStorage => O:16:"SplObjectStorage":2:{i:0;a:2:{i:0;O:8:"stdClass":0:{}i:1;s:1:"i";}i:1;a:0:{}}
SplStack => O:8:"SplStack":3:{i:0;i:6;i:1;a:0:{}i:2;a:0:{}}
SplFixedArray => O:13:"SplFixedArray":2:{i:0;N;i:1;N;}
-- __serialize()/__unserialize() called directly agree with it
SplFixedArray::__serialize => {"0":null,"1":"z","p":4}
__unserialize => count=2 [0]=a p=5
