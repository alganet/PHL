--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RecursiveIteratorIterator walks php's five-state machine, one state per level
--DESCRIPTION--
php's spl_recursive_it_object is a STACK OF LEVELS and each level carries its own
RecursiveIteratorState; move_forward() is one loop over that pair. The embedded PHP
kept a stack of iterators with the state implied by two booleans, and four traversal
rules fell out of it: LEAVES_ONLY past max depth YIELDED the container php skips,
the mode was masked with & 3 so CATCH_GET_CHILD passed as $mode descended like
LEAVES_ONLY where php compares the mode exactly and an unrecognized one descends
nowhere, getChildren() answering a non-RecursiveIterator was treated as "no
children" where php throws UnexpectedValueException, and CATCH_GET_CHILD kept the
element whose getChildren() raised where php drops it.
--FILE--
<?php
function rritShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function rritTree() {
    return new RecursiveArrayIterator(['a' => 1, 'b' => ['c' => 2, 'd' => ['e' => 3]], 'f' => 4]);
}
function rritWalk($mode, $flags = 0, $maxDepth = null) {
    $it = new RecursiveIteratorIterator(rritTree(), $mode, $flags);
    if ($maxDepth !== null) { $it->setMaxDepth($maxDepth); }
    $out = [];
    foreach ($it as $k => $v) {
        $out[] = $it->getDepth() . ":$k=" . (is_array($v) ? 'ARR' : $v);
    }
    return implode(' ', $out);
}

rritShow('leaves', fn() => rritWalk(RecursiveIteratorIterator::LEAVES_ONLY));
rritShow('self first', fn() => rritWalk(RecursiveIteratorIterator::SELF_FIRST));
rritShow('child first', fn() => rritWalk(RecursiveIteratorIterator::CHILD_FIRST));

/* A container past max depth is NOT a leaf, so LEAVES_ONLY skips it outright —
 * the mode's defining rule. The other two modes yield it undescended. */
rritShow('leaves maxdepth 0', fn() => rritWalk(RecursiveIteratorIterator::LEAVES_ONLY, 0, 0));
rritShow('leaves maxdepth 1', fn() => rritWalk(RecursiveIteratorIterator::LEAVES_ONLY, 0, 1));
rritShow('self first maxdepth 0', fn() => rritWalk(RecursiveIteratorIterator::SELF_FIRST, 0, 0));
rritShow('child first maxdepth 1', fn() => rritWalk(RecursiveIteratorIterator::CHILD_FIRST, 0, 1));

/* php compares the mode EXACTLY: 16 is CATCH_GET_CHILD's value, not a mode, and
 * matches no arm — so nothing is descended into. */
rritShow('mode 16 descends nowhere', fn() => rritWalk(16));
rritShow('mode 99 descends nowhere', fn() => rritWalk(99));

rritShow('maxdepth default', fn() => (new RecursiveIteratorIterator(rritTree()))->getMaxDepth());
rritShow('setMaxDepth() resets', function () {
    $it = new RecursiveIteratorIterator(rritTree());
    $it->setMaxDepth(3);
    $before = $it->getMaxDepth();
    $it->setMaxDepth();
    return [$before, $it->getMaxDepth()];
});
rritShow('setMaxDepth(-2)', fn() => (new RecursiveIteratorIterator(rritTree()))->setMaxDepth(-2));

class RritBadChildren implements RecursiveIterator {
    private $i = 0;
    private $d;
    public function __construct($d = [1, 2]) { $this->d = $d; }
    public function current(): mixed { return $this->d[$this->i]; }
    public function key(): mixed { return $this->i; }
    public function next(): void { $this->i++; }
    public function rewind(): void { $this->i = 0; }
    public function valid(): bool { return $this->i < count($this->d); }
    public function hasChildren(): bool { return $this->i === 0; }
    public function getChildren(): ?RecursiveIterator { return null; }
}
class RritThrowChildren extends RritBadChildren {
    public function getChildren(): ?RecursiveIterator { throw new RuntimeException('boom'); }
}

rritShow('getChildren() must be a RecursiveIterator', function () {
    $out = [];
    foreach (new RecursiveIteratorIterator(new RritBadChildren()) as $v) { $out[] = $v; }
    return $out;
});
rritShow('a throwing getChildren() propagates', function () {
    $out = [];
    foreach (new RecursiveIteratorIterator(new RritThrowChildren()) as $v) { $out[] = $v; }
    return $out;
});
/* CATCH_GET_CHILD clears the exception and moves ON: the element whose children
 * could not be fetched is dropped, not kept. */
rritShow('CATCH_GET_CHILD drops the element', function () {
    $out = [];
    $it = new RecursiveIteratorIterator(new RritThrowChildren(),
        RecursiveIteratorIterator::LEAVES_ONLY, RecursiveIteratorIterator::CATCH_GET_CHILD);
    foreach ($it as $v) { $out[] = $v; }
    return $out;
});

rritShow('an aggregate is unwrapped once', function () {
    $agg = new class implements IteratorAggregate {
        public function getIterator(): Traversable { return new RecursiveArrayIterator([1, ['x' => 2]]); }
    };
    $it = new RecursiveIteratorIterator($agg);
    $out = [];
    foreach ($it as $k => $v) { $out[] = "$k=$v"; }
    return [implode(' ', $out), get_class($it->getInnerIterator())];
});
rritShow('a plain Iterator is refused', fn() => new RecursiveIteratorIterator(new ArrayIterator([1])));
rritShow('an array is refused', fn() => new RecursiveIteratorIterator([1]));
--EXPECT--
leaves => '0:a=1 1:c=2 2:e=3 0:f=4'
self first => '0:a=1 0:b=ARR 1:c=2 1:d=ARR 2:e=3 0:f=4'
child first => '0:a=1 1:c=2 2:e=3 1:d=ARR 0:b=ARR 0:f=4'
leaves maxdepth 0 => '0:a=1 0:f=4'
leaves maxdepth 1 => '0:a=1 1:c=2 0:f=4'
self first maxdepth 0 => '0:a=1 0:b=ARR 0:f=4'
child first maxdepth 1 => '0:a=1 1:c=2 1:d=ARR 0:b=ARR 0:f=4'
mode 16 descends nowhere => '0:a=1 0:b=ARR 0:f=4'
mode 99 descends nowhere => '0:a=1 0:b=ARR 0:f=4'
maxdepth default => false
setMaxDepth() resets => array (  0 => 3,  1 => false,)
setMaxDepth(-2) => ValueError: RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) must be greater than or equal to -1
getChildren() must be a RecursiveIterator => UnexpectedValueException: Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator
a throwing getChildren() propagates => RuntimeException: boom
CATCH_GET_CHILD drops the element => array (  0 => 2,)
an aggregate is unwrapped once => array (  0 => '0=1 x=2',  1 => 'RecursiveArrayIterator',)
a plain Iterator is refused => InvalidArgumentException: An instance of RecursiveIterator or IteratorAggregate creating it is required
an array is refused => TypeError: RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) must be of type object, array given
