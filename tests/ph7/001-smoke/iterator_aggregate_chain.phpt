--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getIterator() may hand back another IteratorAggregate, and the whole chain resolves
--FILE--
<?php
/* php resolves an IteratorAggregate CHAIN: whatever getIterator() hands back is
 * asked the same question, so an aggregate may return another aggregate and only
 * the Iterator at the end drives the loop. The two ends php refuses are a
 * receiver that returns ITSELF and a return that is not Traversable — both with
 * a catchable Exception naming the receiver. */
class ItcArr implements IteratorAggregate
{
    public function __construct(private array $a) {}
    public function getIterator(): Traversable { return new ArrayIterator($this->a); }
}
class ItcOnce implements IteratorAggregate
{
    public function getIterator(): Traversable { return new ItcArr([1, 2]); }
}
class ItcDeep implements IteratorAggregate
{
    public function __construct(private int $n) {}
    public function getIterator(): Traversable
    {
        return $this->n > 0 ? new ItcDeep($this->n - 1) : new ArrayIterator([7, 8]);
    }
}
class ItcGen implements IteratorAggregate
{
    public function getIterator(): Traversable { return (function () { yield 9; yield 10; })(); }
}
class ItcOfGen implements IteratorAggregate
{
    public function getIterator(): Traversable { return new ItcGen(); }
}
class ItcSelf implements IteratorAggregate
{
    public function getIterator(): Traversable { return $this; }
}

$itcShow = function (string $tag, callable $fn) {
    try { echo $tag, ' ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo $tag, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
};

$itcCollect = function (iterable $it) { $o = []; foreach ($it as $k => $v) { $o[] = "$k=$v"; } return $o; };

$itcShow('one-hop   ', fn () => $itcCollect(new ItcArr(['a' => 1])));
$itcShow('two-hop   ', fn () => $itcCollect(new ItcOnce()));
$itcShow('deep      ', fn () => $itcCollect(new ItcDeep(5)));
$itcShow('generator ', fn () => $itcCollect(new ItcGen()));
$itcShow('agg-of-gen', fn () => $itcCollect(new ItcOfGen()));
$itcShow('self      ', fn () => $itcCollect(new ItcSelf()));

/* The iterator_*() family and the spread go through the same resolution, so they
 * have to agree with foreach for the same value. */
$itcShow('to_array  ', fn () => iterator_to_array(new ItcOfGen(), false));
$itcShow('count     ', fn () => iterator_count(new ItcOnce()));
$itcShow('spread    ', fn () => [...new ItcOnce()]);
$itcShow('to_array2 ', fn () => iterator_to_array(new ItcSelf()));
$itcShow('in_array  ', fn () => in_array(2, iterator_to_array(new ItcOnce()), true));

/* An aggregate whose getIterator() throws is still the throw, not a refusal. */
class ItcThrow implements IteratorAggregate
{
    public function getIterator(): Traversable { throw new RuntimeException('from getIterator'); }
}
class ItcOfThrow implements IteratorAggregate
{
    public function getIterator(): Traversable { return new ItcThrow(); }
}
$itcShow('throws    ', fn () => $itcCollect(new ItcThrow()));
$itcShow('throws2   ', fn () => $itcCollect(new ItcOfThrow()));
$itcApplyN = 0;
$itcShow('apply     ', function () use (&$itcApplyN) {
    iterator_apply(new ItcOnce(), function () use (&$itcApplyN) { $itcApplyN++; return true; });
    return $itcApplyN;
});
?>
--EXPECT--
one-hop    ["a=1"]
two-hop    ["0=1","1=2"]
deep       ["0=7","1=8"]
generator  ["0=9","1=10"]
agg-of-gen ["0=9","1=10"]
self       self       Exception: Objects returned by ItcSelf::getIterator() must be traversable or implement interface Iterator
to_array   [9,10]
count      2
spread     [1,2]
to_array2  to_array2  Exception: Objects returned by ItcSelf::getIterator() must be traversable or implement interface Iterator
in_array   true
throws     throws     RuntimeException: from getIterator
throws2    throws2    RuntimeException: from getIterator
apply      2
