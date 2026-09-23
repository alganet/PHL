--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RecursiveIteratorIterator seeds level 0 in the constructor and hooks the walk
--DESCRIPTION--
php's constructor seeds iterators[0], so getDepth(), getSubIterator() and
getInnerIterator() answer BEFORE any rewind() and valid() is already true over a
non-empty iterator — it ASKS the levels rather than reading a "started" flag. The
seven overridable hooks are php's documented interception points: callHasChildren()
is what the traversal asks (not the sub-iterator's hasChildren() directly),
endChildren() runs BEFORE the level is popped so it reports the depth it is
leaving, and the in_iteration latch fires beginIteration() once no matter how often
rewind() is called. An instance whose parent constructor never ran refuses EVERY
method with php's Error, which is a class-wide handler rather than a per-body check.
--FILE--
<?php
function rrihShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
function rrihTree() {
    return new RecursiveArrayIterator(['a' => 1, 'b' => ['c' => 2, 'd' => ['e' => 3]], 'f' => 4]);
}

/* Level 0 exists from the constructor: nothing below needs a rewind() first. */
rrihShow('depth before rewind', fn() => (new RecursiveIteratorIterator(rrihTree()))->getDepth());
rrihShow('valid before rewind', fn() => (new RecursiveIteratorIterator(rrihTree()))->valid());
rrihShow('sub iterator before rewind',
    fn() => get_class((new RecursiveIteratorIterator(rrihTree()))->getSubIterator()));
rrihShow('inner iterator before rewind',
    fn() => get_class((new RecursiveIteratorIterator(rrihTree()))->getInnerIterator()));
rrihShow('sub iterator level 0',
    fn() => get_class((new RecursiveIteratorIterator(rrihTree()))->getSubIterator(0)));
rrihShow('sub iterator out of range',
    fn() => (new RecursiveIteratorIterator(rrihTree()))->getSubIterator(5));
rrihShow('sub iterator negative',
    fn() => (new RecursiveIteratorIterator(rrihTree()))->getSubIterator(-1));

/* php keeps level 0 addressable after exhaustion — the stack is never emptied. */
rrihShow('after exhaustion', function () {
    $it = new RecursiveIteratorIterator(rrihTree());
    foreach ($it as $v) {}
    return [$it->getDepth(), $it->valid(), get_class($it->getSubIterator())];
});

class RrihHooked extends RecursiveIteratorIterator {
    public $log = [];
    public function beginIteration(): void { $this->log[] = 'beginIteration'; }
    public function endIteration(): void { $this->log[] = 'endIteration'; }
    public function beginChildren(): void { $this->log[] = 'beginChildren@' . $this->getDepth(); }
    public function endChildren(): void { $this->log[] = 'endChildren@' . $this->getDepth(); }
    public function nextElement(): void { $this->log[] = 'nextElement:' . $this->key(); }
    public function callHasChildren(): bool { $this->log[] = 'callHasChildren'; return parent::callHasChildren(); }
    public function callGetChildren(): ?RecursiveIterator { $this->log[] = 'callGetChildren'; return parent::callGetChildren(); }
}

/* callHasChildren() is asked for every element, and endChildren() reports the
 * depth it is LEAVING because php calls it before the pop. */
rrihShow('hook order', function () {
    $it = new RrihHooked(rrihTree());
    foreach ($it as $v) {}
    return implode(' ', $it->log);
});
/* The in_iteration latch: rewind() twice announces the iteration once. */
rrihShow('beginIteration fires once', function () {
    $it = new RrihHooked(rrihTree());
    $it->rewind();
    $it->rewind();
    return implode(' ', $it->log);
});

/* php's callHasChildren()/callGetChildren() ask the CURRENT level's iterator, so
 * they are live before the first rewind() too — here getChildren() wraps the
 * scalar 1 and RecursiveArrayIterator refuses it. */
rrihShow('callHasChildren before rewind',
    fn() => (new RecursiveIteratorIterator(rrihTree()))->callHasChildren());
rrihShow('callGetChildren before rewind',
    fn() => (new RecursiveIteratorIterator(rrihTree()))->callGetChildren());

class RrihNoParentCtor extends RecursiveIteratorIterator {
    public function __construct() {}
}
rrihShow('uninitialized getDepth', fn() => (new RrihNoParentCtor())->getDepth());
rrihShow('uninitialized valid', fn() => (new RrihNoParentCtor())->valid());
rrihShow('uninitialized rewind', fn() => (new RrihNoParentCtor())->rewind());
rrihShow('uninitialized getInnerIterator', fn() => (new RrihNoParentCtor())->getInnerIterator());
rrihShow('uninitialized callHasChildren', fn() => (new RrihNoParentCtor())->callHasChildren());

/* php presents no properties at all and refuses clone. */
rrihShow('properties', function () {
    $names = [];
    foreach ((new ReflectionClass('RecursiveIteratorIterator'))->getProperties() as $p) {
        $names[] = $p->getName();
    }
    return [$names, get_object_vars(new RecursiveIteratorIterator(rrihTree())),
        (array)(new RecursiveIteratorIterator(rrihTree()))];
});
rrihShow('clone', fn() => clone new RecursiveIteratorIterator(rrihTree()));
--EXPECT--
depth before rewind => 0
valid before rewind => true
sub iterator before rewind => 'RecursiveArrayIterator'
inner iterator before rewind => 'RecursiveArrayIterator'
sub iterator level 0 => 'RecursiveArrayIterator'
sub iterator out of range => NULL
sub iterator negative => NULL
after exhaustion => array (  0 => 0,  1 => false,  2 => 'RecursiveArrayIterator',)
hook order => 'beginIteration callHasChildren nextElement:a callHasChildren callGetChildren beginChildren@1 callHasChildren nextElement:c callHasChildren callGetChildren beginChildren@2 callHasChildren nextElement:e endChildren@2 endChildren@1 callHasChildren nextElement:f endIteration'
beginIteration fires once => 'beginIteration callHasChildren nextElement:a callHasChildren nextElement:a'
callHasChildren before rewind => false
callGetChildren before rewind => TypeError: ArrayIterator::__construct(): Argument #1 ($array) must be of type array, int given
uninitialized getDepth => Error: The RrihNoParentCtor instance wasn't initialized properly
uninitialized valid => Error: The RrihNoParentCtor instance wasn't initialized properly
uninitialized rewind => Error: The RrihNoParentCtor instance wasn't initialized properly
uninitialized getInnerIterator => Error: The RrihNoParentCtor instance wasn't initialized properly
uninitialized callHasChildren => Error: The RrihNoParentCtor instance wasn't initialized properly
properties => array (  0 =>   array (  ),  1 =>   array (  ),  2 =>   array (  ),)
clone => Error: Trying to clone an uncloneable object of class RecursiveIteratorIterator
