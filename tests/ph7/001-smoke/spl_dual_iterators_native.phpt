--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The SPL decorator iterators are C classes and cache what they fetched
--DESCRIPTION--
php's dual iterator is a CACHE: rewind() and next() move the inner iterator and
COPY its current()/key() onto the decorator, and valid()/current()/key() answer
out of that copy. The embedded PHP forwarded all five live, so a decorator was
valid() before it had ever been rewound, and it followed an inner iterator that
had been moved behind its back. Converting also DECLARED the parameters php
declares: only IteratorIterator takes a Traversable (and unwraps one level of
IteratorAggregate, keeping the aggregate as its inner object) — every other
decorator takes an Iterator and refuses an aggregate.
--FILE--
<?php
function sdinShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
class SdinAgg implements IteratorAggregate {
    private $a;
    public function __construct($a) { $this->a = $a; }
    public function getIterator(): Iterator { return new ArrayIterator($this->a); }
}
class SdinAggOfAgg implements IteratorAggregate {
    public function getIterator(): Traversable { return new SdinAgg([7, 8]); }
}
class SdinOdd extends FilterIterator {
    public function accept(): bool { return $this->current() % 2 === 1; }
}
class SdinNoParent extends IteratorIterator {
    public function __construct() {}
}

/* The cache is the class: nothing is fetched until rewind(). */
sdinShow('before rewind', function () {
    $i = new IteratorIterator(new ArrayIterator([1, 2]));
    return [$i->valid(), $i->current(), $i->key()];
});
sdinShow('cached current', function () {
    $a = new ArrayIterator([1, 2, 3]);
    $i = new IteratorIterator($a);
    $i->rewind();
    $a->next();                     /* the inner moves behind the decorator's back */
    return [$i->current(), $i->key(), $a->current()];
});
sdinShow('past the end', function () {
    $i = new IteratorIterator(new ArrayIterator([1]));
    $i->rewind(); $i->next();
    return [$i->valid(), $i->current(), $i->key()];
});
sdinShow('iterate', fn() => iterator_to_array(new IteratorIterator(new ArrayIterator(['x' => 1, 'y' => 2]))));

/* Traversable, one level of unwrapping, and the downcast argument. */
sdinShow('aggregate inner', function () {
    $i = new IteratorIterator(new SdinAgg([5, 6]));
    return [get_class($i->getInnerIterator()), iterator_to_array($i)];
});
sdinShow('aggregate of aggregate', function () {
    $i = new IteratorIterator(new SdinAggOfAgg());
    return [get_class($i->getInnerIterator()), iterator_to_array($i)];
});
sdinShow('downcast not a base', fn() => new IteratorIterator(new SdinAgg([1]), 'ArrayObject'));
sdinShow('downcast not traversable', fn() => new IteratorIterator(new SdinAgg([1]), 'stdClass'));
sdinShow('array is not Traversable', fn() => new IteratorIterator([1, 2]));

/* Everything else declares Iterator, so an aggregate is refused by name. */
sdinShow('limit refuses aggregate', fn() => new LimitIterator(new SdinAgg([1])));
sdinShow('filter refuses aggregate', fn() => new SdinOdd(new SdinAgg([1])));
sdinShow('norewind refuses aggregate', fn() => new NoRewindIterator(new SdinAgg([1])));

/* The two states php refuses outright. */
sdinShow('no parent ctor', function () { $x = new SdinNoParent(); return $x->valid(); });
sdinShow('built twice', function () {
    $i = new LimitIterator(new ArrayIterator([1]));
    $i->__construct(new ArrayIterator([2]));
});
sdinShow('uncloneable', fn() => clone new IteratorIterator(new ArrayIterator([1])));

/* LimitIterator: the window, the position, and the seek refusals. */
sdinShow('limit window', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3, 4, 5]), 2, 3);
    $out = [];
    foreach ($l as $k => $v) { $out[] = "$k=$v@" . $l->getPosition(); }
    return $out;
});
sdinShow('limit past window', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3]), 0, 2);
    $l->rewind(); $l->next(); $l->next();
    return [$l->valid(), $l->current(), $l->getPosition()];
});
sdinShow('limit seek', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3, 4, 5]), 1, 3);
    return [$l->seek(2), $l->current(), $l->getPosition()];
});
sdinShow('limit seek below', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3, 4, 5]), 1, 3);
    $l->seek(0);
});
sdinShow('limit seek behind', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3, 4, 5]), 1, 3);
    $l->seek(4);
});
sdinShow('limit seek backward', function () {
    $l = new LimitIterator(new ArrayIterator([0, 1, 2, 3, 4]), 0, 4);
    $l->rewind(); $l->next(); $l->next();
    return [$l->getPosition(), $l->seek(0), $l->current()];
});
sdinShow('limit offset value', fn() => new LimitIterator(new ArrayIterator([1]), -1));
sdinShow('limit limit value', fn() => new LimitIterator(new ArrayIterator([1]), 0, -2));
sdinShow('limit offset type', fn() => new LimitIterator(new ArrayIterator([1]), 'x'));

/* FilterIterator and its callback twin. */
sdinShow('filter', fn() => iterator_to_array(new SdinOdd(new ArrayIterator([1, 2, 3, 4, 5]))));
sdinShow('filter is abstract', fn() => new FilterIterator(new ArrayIterator([1])));
sdinShow('callback filter', function () {
    $seen = [];
    $c = new CallbackFilterIterator(new ArrayIterator(['a' => 1, 'b' => 2]),
        function ($v, $k, $it) use (&$seen) { $seen[] = "$k=$v:" . get_class($it); return $v > 1; });
    return [iterator_to_array($c), $seen];
});
sdinShow('callback filter refusal', fn() => new CallbackFilterIterator(new ArrayIterator([1]), 'sdinNope'));

/* InfiniteIterator, NoRewindIterator, EmptyIterator. */
sdinShow('infinite', function () {
    $out = [];
    $n = 0;
    foreach (new InfiniteIterator(new ArrayIterator([1, 2])) as $v) { $out[] = $v; if (++$n >= 5) break; }
    return $out;
});
sdinShow('infinite empty', function () {
    $i = new InfiniteIterator(new ArrayIterator([]));
    $i->rewind();
    return $i->valid();
});
sdinShow('norewind keeps position', function () {
    $a = new ArrayIterator([1, 2, 3]);
    $a->next();
    return iterator_to_array(new NoRewindIterator($a), false);
});
sdinShow('norewind reads live', function () {
    $a = new ArrayIterator([1, 2]);
    $n = new NoRewindIterator($a);
    return [$n->valid(), $n->current(), $n->key()];
});
sdinShow('empty valid', function () { $e = new EmptyIterator(); return [$e->valid(), iterator_to_array($e)]; });
sdinShow('empty current', fn() => (new EmptyIterator())->current());
sdinShow('empty key', fn() => (new EmptyIterator())->key());
sdinShow('empty clones', fn() => get_class(clone new EmptyIterator()));

/* The declarations themselves. */
sdinShow('hierarchy', fn() => [
    get_parent_class('LimitIterator'),
    get_parent_class('CallbackFilterIterator'),
    (new ReflectionClass('FilterIterator'))->isAbstract(),
    interface_exists('OuterIterator'),
    (new ReflectionMethod('LimitIterator', 'seek'))->getNumberOfParameters(),
]);
--EXPECT--
before rewind => array (  0 => false,  1 => NULL,  2 => NULL,)
cached current => array (  0 => 1,  1 => 0,  2 => 2,)
past the end => array (  0 => false,  1 => NULL,  2 => NULL,)
iterate => array (  'x' => 1,  'y' => 2,)
aggregate inner => array (  0 => 'ArrayIterator',  1 =>   array (    0 => 5,    1 => 6,  ),)
aggregate of aggregate => array (  0 => 'SdinAgg',  1 =>   array (    0 => 7,    1 => 8,  ),)
downcast not a base => LogicException: Class to downcast to not found or not base class or does not implement Traversable
downcast not traversable => LogicException: Class to downcast to not found or not base class or does not implement Traversable
array is not Traversable => TypeError: IteratorIterator::__construct(): Argument #1 ($iterator) must be of type Traversable, array given
limit refuses aggregate => TypeError: LimitIterator::__construct(): Argument #1 ($iterator) must be of type Iterator, SdinAgg given
filter refuses aggregate => TypeError: FilterIterator::__construct(): Argument #1 ($iterator) must be of type Iterator, SdinAgg given
norewind refuses aggregate => TypeError: NoRewindIterator::__construct(): Argument #1 ($iterator) must be of type Iterator, SdinAgg given
no parent ctor => Error: The object is in an invalid state as the parent constructor was not called
built twice => BadMethodCallException: LimitIterator::getIterator() must be called exactly once per instance
uncloneable => Error: Trying to clone an uncloneable object of class IteratorIterator
limit window => array (  0 => '2=2@2',  1 => '3=3@3',  2 => '4=4@4',)
limit past window => array (  0 => false,  1 => NULL,  2 => 2,)
limit seek => array (  0 => 2,  1 => 2,  2 => 2,)
limit seek below => OutOfBoundsException: Cannot seek to 0 which is below the offset 1
limit seek behind => OutOfBoundsException: Cannot seek to 4 which is behind offset 1 plus count 3
limit seek backward => array (  0 => 2,  1 => 0,  2 => 0,)
limit offset value => ValueError: LimitIterator::__construct(): Argument #2 ($offset) must be greater than or equal to 0
limit limit value => ValueError: LimitIterator::__construct(): Argument #3 ($limit) must be greater than or equal to -1
limit offset type => TypeError: LimitIterator::__construct(): Argument #2 ($offset) must be of type int, string given
filter => array (  0 => 1,  2 => 3,  4 => 5,)
filter is abstract => Error: Cannot instantiate abstract class FilterIterator
callback filter => array (  0 =>   array (    'b' => 2,  ),  1 =>   array (    0 => 'a=1:ArrayIterator',    1 => 'b=2:ArrayIterator',  ),)
callback filter refusal => TypeError: CallbackFilterIterator::__construct(): Argument #2 ($callback) must be a valid callback, function "sdinNope" not found or invalid function name
infinite => array (  0 => 1,  1 => 2,  2 => 1,  3 => 2,  4 => 1,)
infinite empty => false
norewind keeps position => array (  0 => 2,  1 => 3,)
norewind reads live => array (  0 => true,  1 => 1,  2 => 0,)
empty valid => array (  0 => false,  1 =>   array (  ),)
empty current => BadMethodCallException: Accessing the value of an EmptyIterator
empty key => BadMethodCallException: Accessing the key of an EmptyIterator
empty clones => 'EmptyIterator'
hierarchy => array (  0 => 'IteratorIterator',  1 => 'FilterIterator',  2 => true,  3 => true,  4 => 1,)
