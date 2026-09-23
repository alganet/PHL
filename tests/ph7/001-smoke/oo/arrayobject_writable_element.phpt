--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A writable container hands back its own element, so an indirect modification lands
--DESCRIPTION--
php's `spl_array_read_dimension` / WeakMap read_dimension answer a fetch with the
STORE's own element rather than a copy of it — that is what makes ArrayObject,
ArrayIterator and WeakMap support indirect modification while a userland
ArrayAccess gets the "has no effect" notice. PHL reached those elements through
`offsetGet`, which returns a VALUE however native it is, so a direct nested store
landed only by COW accident and every shape that needs a real SLOT wrote into a
temporary and left the store as it was, in silence: `$r = &$ao['a']`,
`sort($ao['a'])`, `unset($ao['a'][0])`, `foreach ($ao['a'] as &$v)` and
`$ao['n']++`. A COMPOUND assign is deliberately not on that path — `$ao[k] op= v`
is php's ASSIGN_DIM_OP on an object, which reads and writes through the accessors.
--FILE--
<?php
function aoWritableDump($label, $v) { echo str_pad($label, 26), json_encode($v), "\n"; }

// --- ArrayObject: every shape that needs the element's own slot.
$ao = new ArrayObject(['a' => [3, 1, 2], 'n' => 5]);
$ao['a']['z'] = 9;              aoWritableDump('nested store', $ao['a']);
$ao['a'][] = 7;                 aoWritableDump('nested append', $ao['a']);
$r = &$ao['a']; $r['q'] = 1;    aoWritableDump('bound by reference', $ao['a']);
unset($r);
sort($ao['a']);                 aoWritableDump('sort() by-ref argument', $ao['a']);
unset($ao['a'][0]);             aoWritableDump('unset of a nested key', $ao['a']);
foreach ($ao['a'] as &$v) { $v = 'X'; }
unset($v);                      aoWritableDump('by-reference foreach', $ao['a']);
$ao['n']++;                     aoWritableDump('increment', $ao['n']);

// A MISSING key is created by a WRITE-context fetch (and only there).
$ao['fresh']['k'] = 1;          aoWritableDump('vivified by a store', $ao['fresh']);
$r2 = &$ao['bound']; $r2 = 5;   aoWritableDump('vivified by a bind', $ao['bound']);
unset($r2);
aoWritableDump('count', count($ao));

// --- ArrayIterator takes the same route, and its cursor is untouched by it.
$ai = new ArrayIterator(['a' => [3, 1, 2]]);
sort($ai['a']);                 aoWritableDump('ArrayIterator sort()', $ai['a']);

// --- WeakMap: the value slot is real too, but a missing key is never created.
$key = new stdClass;
$wm = new WeakMap();
$wm[$key] = [3, 1, 2];
sort($wm[$key]);                aoWritableDump('WeakMap sort()', $wm[$key]);
$r3 = &$wm[$key]; $r3[] = 'R';  aoWritableDump('WeakMap by reference', $wm[$key]);
unset($r3);
try {
    $absent = new stdClass;
    $wm[$absent]['k'] = 1;
} catch (Error $e) {
    echo "absent WeakMap key: ", get_class($e), "\n";
}

// --- A compound assign stays with the accessors: a subclass overriding only
// offsetSet sees its own method called for `op=` and NOT for `++`.
class AoWritableSub extends ArrayObject {
    public function offsetSet($k, $v): void { echo "[set]"; parent::offsetSet($k, $v); }
}
$sub = new AoWritableSub(['n' => 5, 'a' => [3, 1]]);
$sub['n'] += 2;                 aoWritableDump(' compound assign', $sub['n']);
$sub['n']++;                    aoWritableDump('increment', $sub['n']);
$sub['a']['x'] = 9;             aoWritableDump('nested store', $sub['a']);
sort($sub['a']);                aoWritableDump('sort() by-ref argument', $sub['a']);

// --- An override of offsetGet takes the subclass OFF the fast path: php's
// read_dimension steps aside for it, so the write is indirect and has no effect.
class AoWritableGetSub extends ArrayObject {
    public function offsetGet($k): mixed { return parent::offsetGet($k); }
}
$got = new AoWritableGetSub(['a' => [3, 1]]);
$aoWritableEr = error_reporting(E_ALL & ~E_NOTICE);
sort($got['a']);
error_reporting($aoWritableEr);
aoWritableDump('overridden offsetGet', $got['a']);

// --- A TEMPORARY container has no slot to hand out and needs none.
aoWritableDump('temporary', (new ArrayObject(['a' => [1, 2]]))['a']);
?>
--EXPECT--
nested store              {"0":3,"1":1,"2":2,"z":9}
nested append             {"0":3,"1":1,"2":2,"z":9,"3":7}
bound by reference        {"0":3,"1":1,"2":2,"z":9,"3":7,"q":1}
sort() by-ref argument    [1,1,2,3,7,9]
unset of a nested key     {"1":1,"2":2,"3":3,"4":7,"5":9}
by-reference foreach      {"1":"X","2":"X","3":"X","4":"X","5":"X"}
increment                 6
vivified by a store       {"k":1}
vivified by a bind        5
count                     4
ArrayIterator sort()      [1,2,3]
WeakMap sort()            [1,2,3]
WeakMap by reference      [1,2,3,"R"]
absent WeakMap key: Error
[set] compound assign          7
increment                 8
nested store              {"0":3,"1":1,"x":9}
sort() by-ref argument    [1,3,9]
overridden offsetGet      [3,1]
temporary                 [1,2]
