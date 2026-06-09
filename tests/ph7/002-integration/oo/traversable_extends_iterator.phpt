--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Iterator and IteratorAggregate transitively implement Traversable
--FILE--
<?php
class It implements Iterator {
    private int $i = 0;
    public function rewind(): void { $this->i = 0; }
    public function valid(): bool { return $this->i < 2; }
    #[\ReturnTypeWillChange]
    public function current() { return $this->i; }
    #[\ReturnTypeWillChange]
    public function key() { return $this->i; }
    public function next(): void { $this->i++; }
}
class Agg implements IteratorAggregate {
    public function getIterator(): Iterator { return new It(); }
}
$it = new It();
$ag = new Agg();
echo "It Iterator: ", $it instanceof Iterator ? "y" : "n", "\n";
echo "It Traversable: ", $it instanceof Traversable ? "y" : "n", "\n";
echo "Agg IteratorAggregate: ", $ag instanceof IteratorAggregate ? "y" : "n", "\n";
echo "Agg Traversable: ", $ag instanceof Traversable ? "y" : "n", "\n";
echo "Agg Iterator: ", $ag instanceof Iterator ? "y" : "n", "\n";
?>
--EXPECT--
It Iterator: y
It Traversable: y
Agg IteratorAggregate: y
Agg Traversable: y
Agg Iterator: n
--CLEAN--
<?php
