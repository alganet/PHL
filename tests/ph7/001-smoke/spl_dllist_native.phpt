--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SplDoublyLinkedList carries php's fix bit, and a LIFO list indexes from the end
--DESCRIPTION--
php's spl_dllist_object is a list plus a FLAGS word, and IT_FIX (4) is the half of
it no constant names: the object handler stamps it at creation for SplStack and
SplQueue, which is both what freezes their LIFO/FIFO choice and why a fresh
SplStack reports mode 6 rather than 2. Everything measured in OFFSETS goes through
php's spl_ptr_llist_offset with that LIFO bit, so `$stack[0]` is what top() answers.
The embedded PHP had neither rule, invented a toArray() php does not have, dropped
the Serializable interface with all four of its serialization methods and
__debugInfo(), reported out-of-range from the RUNTIME class instead of the
declaring one, and let a non-numeric offset coerce to 0 instead of raising.
--FILE--
<?php
function sdlShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function sdlList($cls = 'SplDoublyLinkedList', array $items = [1, 2, 3]) {
    $l = new $cls();
    foreach ($items as $i) { $l->push($i); }
    return $l;
}
function sdlDump($l) { return iterator_to_array(clone $l, false); }

/* IT_FIX is stamped at creation, has no constant, and survives setIteratorMode. */
sdlShow('modes', fn() => [(new SplDoublyLinkedList())->getIteratorMode(),
    (new SplStack())->getIteratorMode(), (new SplQueue())->getIteratorMode()]);
sdlShow('setIteratorMode answers the stored word', function () {
    $l = sdlList();
    return [$l->setIteratorMode(SplDoublyLinkedList::IT_MODE_DELETE), $l->getIteratorMode()];
});
sdlShow('the fix bit survives', function () {
    $s = sdlList('SplStack');
    return $s->setIteratorMode(SplDoublyLinkedList::IT_MODE_LIFO
        | SplDoublyLinkedList::IT_MODE_DELETE);
});
/* php masks the value to the two mode bits rather than refusing a nonsense one. */
sdlShow('a nonsense mode is masked', fn() => sdlList()->setIteratorMode(99));
sdlShow('stack LIFO is frozen', fn() => sdlList('SplStack')->setIteratorMode(0));
sdlShow('queue FIFO is frozen', fn() => sdlList('SplQueue')->setIteratorMode(2));

/* Every ArrayAccess offset counts from the END of a LIFO list. */
sdlShow('stack offsets run from the top', function () {
    $s = sdlList('SplStack');
    return [$s[0], $s[2], $s->top(), $s->bottom()];
});
sdlShow('list offsets run from the bottom', function () {
    $l = sdlList();
    return [$l[0], $l[2], $l->top(), $l->bottom()];
});
sdlShow('offsetUnset on a stack drops from the top', function () {
    $s = sdlList('SplStack');
    unset($s[0]);
    return sdlDump($s);
});

/* php words an out-of-range offset from the DECLARING class, never the runtime one. */
sdlShow('out of range names the declaring class', fn() => sdlList('SplStack')->add(9, 'x'));
sdlShow('offsetGet out of range', fn() => sdlList()->offsetGet(9));
sdlShow('offsetSet out of range', fn() => sdlList()->offsetSet(9, 'x'));
sdlShow('offsetUnset out of range', fn() => sdlList()->offsetUnset(9));

/* The offsets are ZPP int, so a non-numeric string raises instead of coercing. */
sdlShow('offsetGet refuses a string', fn() => sdlList()->offsetGet('x'));
sdlShow('offsetExists refuses a string', fn() => sdlList()->offsetExists('x'));
sdlShow('add refuses a string', fn() => sdlList()->add('zz', 'x'));
sdlShow('a numeric string is fine', fn() => sdlList()->offsetGet('1'));

sdlShow('empty refusals', function () {
    $out = [];
    foreach (['pop', 'shift', 'top', 'bottom'] as $m) {
        try { (new SplDoublyLinkedList())->$m(); }
        catch (Throwable $e) { $out[] = $e->getMessage(); }
    }
    return $out;
});

/* php has no toArray() on any of the three. */
sdlShow('toArray does not exist', fn() => method_exists('SplDoublyLinkedList', 'toArray'));

/* IT_MODE_DELETE consumes as it walks, and in FIFO order php deliberately does
 * not advance the position — every element of a consuming walk reports key 0. */
sdlShow('consuming walk fifo', function () {
    $l = sdlList();
    $l->setIteratorMode(SplDoublyLinkedList::IT_MODE_DELETE);
    $out = [];
    foreach ($l as $k => $v) { $out[] = "$k=$v"; }
    return [implode(' ', $out), count($l)];
});
sdlShow('consuming walk lifo', function () {
    $s = sdlList('SplStack');
    $s->setIteratorMode(SplDoublyLinkedList::IT_MODE_LIFO
        | SplDoublyLinkedList::IT_MODE_DELETE);
    $out = [];
    foreach ($s as $k => $v) { $out[] = "$k=$v"; }
    return [implode(' ', $out), count($s)];
});

/* Serializable plus the __serialize pair php actually uses. */
sdlShow('is Serializable', fn() => in_array('Serializable',
    class_implements('SplDoublyLinkedList'), true));
sdlShow('serialize', fn() => serialize(sdlList()));
sdlShow('serialize keeps the fix bit', fn() => serialize(sdlList('SplStack')));
sdlShow('__serialize', fn() => sdlList()->__serialize());
sdlShow('the Serializable format', fn() => sdlList()->serialize());
sdlShow('round trip', function () {
    $l = sdlList();
    $l->setIteratorMode(SplDoublyLinkedList::IT_MODE_DELETE);
    $r = unserialize(serialize($l));
    return [sdlDump($r), $r->getIteratorMode(), get_class($r)];
});
sdlShow('stack round trip', function () {
    $r = unserialize(serialize(sdlList('SplStack')));
    return [sdlDump($r), $r->getIteratorMode()];
});

/* php presents flags + dllist and shows NOTHING to the (array) cast. */
/* php's debug keys are the MANGLED private form; PHL presents them unlabelled
 * (the visibility-labelled presented entry is §7.4's open item), so this pins the
 * shape both engines agree on: two entries, the flags then the elements. */
sdlShow('debugInfo values', function () {
    $d = array_values(sdlList()->__debugInfo());
    return [count($d), $d[0], $d[1]];
});
sdlShow('cast is empty', fn() => (array)sdlList());
sdlShow('get_object_vars is empty', fn() => get_object_vars(sdlList()));
sdlShow('no declared properties', fn() => (new ReflectionClass('SplDoublyLinkedList'))->getProperties());
sdlShow('clone is independent', function () {
    $l = sdlList();
    $c = clone $l;
    $c->push(9);
    return [count($l), count($c)];
});
--EXPECT--
modes => array (  0 => 0,  1 => 6,  2 => 4,)
setIteratorMode answers the stored word => array (  0 => 1,  1 => 1,)
the fix bit survives => 7
a nonsense mode is masked => 3
stack LIFO is frozen => RuntimeException: Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen
queue FIFO is frozen => RuntimeException: Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen
stack offsets run from the top => array (  0 => 3,  1 => 1,  2 => 3,  3 => 1,)
list offsets run from the bottom => array (  0 => 1,  1 => 3,  2 => 3,  3 => 1,)
offsetUnset on a stack drops from the top => array (  0 => 2,  1 => 1,)
out of range names the declaring class => OutOfRangeException: SplDoublyLinkedList::add(): Argument #1 ($index) is out of range
offsetGet out of range => OutOfRangeException: SplDoublyLinkedList::offsetGet(): Argument #1 ($index) is out of range
offsetSet out of range => OutOfRangeException: SplDoublyLinkedList::offsetSet(): Argument #1 ($index) is out of range
offsetUnset out of range => OutOfRangeException: SplDoublyLinkedList::offsetUnset(): Argument #1 ($index) is out of range
offsetGet refuses a string => TypeError: SplDoublyLinkedList::offsetGet(): Argument #1 ($index) must be of type int, string given
offsetExists refuses a string => TypeError: SplDoublyLinkedList::offsetExists(): Argument #1 ($index) must be of type int, string given
add refuses a string => TypeError: SplDoublyLinkedList::add(): Argument #1 ($index) must be of type int, string given
a numeric string is fine => 2
empty refusals => array (  0 => 'Can\'t pop from an empty datastructure',  1 => 'Can\'t shift from an empty datastructure',  2 => 'Can\'t peek at an empty datastructure',  3 => 'Can\'t peek at an empty datastructure',)
toArray does not exist => false
consuming walk fifo => array (  0 => '0=1 0=2 0=3',  1 => 0,)
consuming walk lifo => array (  0 => '2=3 1=2 0=1',  1 => 0,)
is Serializable => true
serialize => 'O:19:"SplDoublyLinkedList":3:{i:0;i:0;i:1;a:3:{i:0;i:1;i:1;i:2;i:2;i:3;}i:2;a:0:{}}'
serialize keeps the fix bit => 'O:8:"SplStack":3:{i:0;i:6;i:1;a:3:{i:0;i:1;i:1;i:2;i:2;i:3;}i:2;a:0:{}}'
__serialize => array (  0 => 0,  1 =>   array (    0 => 1,    1 => 2,    2 => 3,  ),  2 =>   array (  ),)
the Serializable format => 'i:0;:i:1;:i:2;:i:3;'
round trip => array (  0 =>   array (    0 => 1,    1 => 2,    2 => 3,  ),  1 => 1,  2 => 'SplDoublyLinkedList',)
stack round trip => array (  0 =>   array (    0 => 3,    1 => 2,    2 => 1,  ),  1 => 6,)
debugInfo values => array (  0 => 2,  1 => 0,  2 =>   array (    0 => 1,    1 => 2,    2 => 3,  ),)
cast is empty => array ()
get_object_vars is empty => array ()
no declared properties => array ()
clone is independent => array (  0 => 3,  1 => 4,)
