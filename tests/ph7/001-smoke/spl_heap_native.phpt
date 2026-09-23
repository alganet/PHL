--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A heap whose compare() throws is CORRUPTED, and says so until it is recovered
--DESCRIPTION--
php's spl_heap_object carries SPL_HEAP_CORRUPTED, set when an exception escapes the
user's compare() mid-sift: the ordering invariant is then unknown, so every
operation that depends on it refuses with "Heap is corrupted, heap properties are
no longer ensured." until recoverFromCorruption() clears the bit. Which operations
refuse is not guessable and is pinned here against the oracle — insert, extract,
top and next do, while count, isEmpty, current, key, valid and __debugInfo keep
answering. The embedded PHP hardcoded isCorrupted() to false, so a throwing
comparator left a silently mis-ordered heap that kept serving. It also carried a
descending __serial field in every SplPriorityQueue node that php does not have —
php's node is exactly {data, priority} — which leaked into serialize(), var_dump()
and the (array) cast; and it lacked __debugInfo(), __serialize() and
__unserialize() entirely.
--FILE--
<?php
function shpShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function shpDrain($h) { $o = []; while (!$h->isEmpty()) { $o[] = $h->extract(); } return $o; }
function shpQueue(array $pairs = [['a', 1], ['b', 3], ['c', 2]]) {
    $q = new SplPriorityQueue();
    foreach ($pairs as [$v, $p]) { $q->insert($v, $p); }
    return $q;
}

class ShpThrowingHeap extends SplHeap {
    public $armed = false;
    protected function compare($a, $b): int {
        if ($this->armed) { throw new RuntimeException('cmp boom'); }
        return $a <=> $b;
    }
}
function shpCorrupted() {
    $h = new ShpThrowingHeap();
    $h->insert(1);
    $h->insert(2);
    $h->armed = true;
    try { $h->insert(3); } catch (Throwable $e) {}
    $h->armed = false;
    return $h;
}

shpShow('the throw propagates and sets the bit', function () {
    $h = new ShpThrowingHeap();
    $h->insert(1);
    $h->armed = true;
    $caught = null;
    try { $h->insert(2); } catch (Throwable $e) { $caught = get_class($e) . ':' . $e->getMessage(); }
    return [$caught, $h->isCorrupted()];
});
/* The element still lands: php finishes the sift with the exception in flight. */
shpShow('the element still lands', fn() => count(shpCorrupted()));
shpShow('what a corrupted heap refuses', function () {
    $out = [];
    foreach (['insert', 'extract', 'top', 'next', '__serialize'] as $m) {
        $h = shpCorrupted();
        try { $m === 'insert' ? $h->insert(9) : $h->$m(); $out[$m] = 'no throw'; }
        catch (Throwable $e) { $out[$m] = $e->getMessage(); }
    }
    return $out;
});
shpShow('what it still answers', function () {
    $out = [];
    foreach (['count', 'isEmpty', 'current', 'key', 'valid', 'isCorrupted'] as $m) {
        $h = shpCorrupted();
        try { $out[$m] = $h->$m(); } catch (Throwable $e) { $out[$m] = 'THROW'; }
    }
    return $out;
});
shpShow('foreach reaches next() and refuses', function () {
    $h = shpCorrupted();
    try { foreach ($h as $v) {} return 'no throw'; }
    catch (Throwable $e) { return $e->getMessage(); }
});
shpShow('recovery clears the bit', function () {
    $h = shpCorrupted();
    return [$h->recoverFromCorruption(), $h->isCorrupted(), $h->top()];
});

/* Ordering, and php's `true` return type on insert. */
shpShow('min heap drains ascending', fn() => shpDrain(new class extends SplMinHeap {
    public function __construct() { foreach ([5, 1, 3, 9, 7] as $x) { $this->insert($x); } }
}));
shpShow('max heap drains descending', fn() => shpDrain(new class extends SplMaxHeap {
    public function __construct() { foreach ([5, 1, 3, 9, 7] as $x) { $this->insert($x); } }
}));
shpShow('insert answers true', fn() => (new SplMinHeap())->insert(1));
shpShow('recoverFromCorruption answers true', fn() => (new SplMinHeap())->recoverFromCorruption());
shpShow('SplHeap is abstract', fn() => new SplHeap());
shpShow('the key counts down', function () {
    $h = new SplMinHeap();
    foreach ([5, 1, 3] as $x) { $h->insert($x); }
    $o = [];
    foreach ($h as $k => $v) { $o[] = "$k=$v"; }
    return [implode(' ', $o), count($h)];
});

/* php's extract flags: masked to two bits, and asking for neither is an error. */
shpShow('default extract flags', fn() => (new SplPriorityQueue())->getExtractFlags());
shpShow('setExtractFlags answers the word', function () {
    $q = new SplPriorityQueue();
    return [$q->setExtractFlags(SplPriorityQueue::EXTR_BOTH), $q->getExtractFlags()];
});
shpShow('a nonsense flag word is masked', fn() => (new SplPriorityQueue())->setExtractFlags(99));
shpShow('no flag at all is refused', fn() => (new SplPriorityQueue())->setExtractFlags(0));
shpShow('extract both', function () {
    $q = shpQueue();
    $q->setExtractFlags(SplPriorityQueue::EXTR_BOTH);
    return $q->extract();
});
shpShow('extract priority', function () {
    $q = shpQueue();
    $q->setExtractFlags(SplPriorityQueue::EXTR_PRIORITY);
    return shpDrain($q);
});
shpShow('extract data', fn() => shpDrain(shpQueue()));

/* php's node is {data, priority} and nothing else. */
shpShow('the queue node shape', fn() => array_keys((new class extends SplPriorityQueue {
    public function __construct() { $this->insert('a', 1); }
})->__serialize()[1]['heap_elements'][0]));
class ShpNamedMin extends SplMinHeap {}
shpShow('heap serialize', function () {
    $h = new ShpNamedMin();
    $h->insert(1);
    return serialize($h);
});
shpShow('queue serialize', fn() => serialize(shpQueue()));
shpShow('queue round trip keeps the flags', function () {
    $q = shpQueue();
    $q->setExtractFlags(SplPriorityQueue::EXTR_PRIORITY);
    $r = unserialize(serialize($q));
    return [shpDrain($r), $r->getExtractFlags()];
});
shpShow('__serialize is [members, state]', function () {
    $s = shpQueue()->__serialize();
    return [count($s), $s[0], array_keys($s[1])];
});

/* php presents flags / isCorrupted / heap and shows nothing to the (array) cast. */
shpShow('debugInfo values', function () {
    $d = array_values(shpQueue()->__debugInfo());
    return [count($d), $d[0], $d[1]];
});
shpShow('cast is empty', fn() => (array)shpQueue());
shpShow('no declared properties', fn() => (new ReflectionClass('SplPriorityQueue'))->getProperties());
shpShow('clone is independent', function () {
    $q = shpQueue();
    $c = clone $q;
    $c->extract();
    return [count($q), count($c)];
});
--EXPECT--
the throw propagates and sets the bit => array (  0 => 'RuntimeException:cmp boom',  1 => true,)
the element still lands => 3
what a corrupted heap refuses => array (  'insert' => 'Heap is corrupted, heap properties are no longer ensured.',  'extract' => 'Heap is corrupted, heap properties are no longer ensured.',  'top' => 'Heap is corrupted, heap properties are no longer ensured.',  'next' => 'Heap is corrupted, heap properties are no longer ensured.',  '__serialize' => 'Heap is corrupted, heap properties are no longer ensured.',)
what it still answers => array (  'count' => 3,  'isEmpty' => false,  'current' => 2,  'key' => 2,  'valid' => true,  'isCorrupted' => true,)
foreach reaches next() and refuses => 'Heap is corrupted, heap properties are no longer ensured.'
recovery clears the bit => array (  0 => true,  1 => false,  2 => 2,)
min heap drains ascending => array (  0 => 1,  1 => 3,  2 => 5,  3 => 7,  4 => 9,)
max heap drains descending => array (  0 => 9,  1 => 7,  2 => 5,  3 => 3,  4 => 1,)
insert answers true => true
recoverFromCorruption answers true => true
SplHeap is abstract => Error: Cannot instantiate abstract class SplHeap
the key counts down => array (  0 => '2=1 1=3 0=5',  1 => 0,)
default extract flags => 1
setExtractFlags answers the word => array (  0 => 3,  1 => 3,)
a nonsense flag word is masked => 3
no flag at all is refused => RuntimeException: Must specify at least one extract flag
extract both => array (  'data' => 'b',  'priority' => 3,)
extract priority => array (  0 => 3,  1 => 2,  2 => 1,)
extract data => array (  0 => 'b',  1 => 'c',  2 => 'a',)
the queue node shape => array (  0 => 'data',  1 => 'priority',)
heap serialize => 'O:11:"ShpNamedMin":2:{i:0;a:0:{}i:1;a:2:{s:5:"flags";i:0;s:13:"heap_elements";a:1:{i:0;i:1;}}}'
queue serialize => 'O:16:"SplPriorityQueue":2:{i:0;a:0:{}i:1;a:2:{s:5:"flags";i:1;s:13:"heap_elements";a:3:{i:0;a:2:{s:4:"data";s:1:"b";s:8:"priority";i:3;}i:1;a:2:{s:4:"data";s:1:"a";s:8:"priority";i:1;}i:2;a:2:{s:4:"data";s:1:"c";s:8:"priority";i:2;}}}}'
queue round trip keeps the flags => array (  0 =>   array (    0 => 3,    1 => 2,    2 => 1,  ),  1 => 2,)
__serialize is [members, state] => array (  0 => 2,  1 =>   array (  ),  2 =>   array (    0 => 'flags',    1 => 'heap_elements',  ),)
debugInfo values => array (  0 => 3,  1 => 1,  2 => false,)
cast is empty => array ()
no declared properties => array ()
clone is independent => array (  0 => 3,  1 => 2,)
