--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Iterator and IteratorAggregate transitively implement Traversable
--FILE--
<?php
class IfaceTraversableIt implements Iterator {
    #[\ReturnTypeWillChange]
    public function current() { return null; }
    #[\ReturnTypeWillChange]
    public function key() { return null; }
    public function next(): void {}
    public function rewind(): void {}
    public function valid(): bool { return false; }
}
class IfaceTraversableAgg implements IteratorAggregate {
    public function getIterator(): Iterator { return new IfaceTraversableIt(); }
}
echo (new IfaceTraversableIt()) instanceof Traversable ? "It:yes" : "It:no", "\n";
echo (new IfaceTraversableAgg()) instanceof Traversable ? "Agg:yes" : "Agg:no", "\n";
?>
--EXPECT--
It:yes
Agg:yes
--CLEAN--
<?php
