--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
iterator_to_array over Generator / Iterator / IteratorAggregate / array
--FILE--
<?php
function itaGen() { yield "a" => 1; yield "b" => 2; }
$r = iterator_to_array(itaGen());
echo $r["a"], $r["b"], "\n";                       // preserve keys (default)
echo implode(",", iterator_to_array(itaGen(), false)), "\n"; // renumber

class ItaRange implements Iterator {
    private $i = 0;
    public function rewind(): void { $this->i = 0; }
    public function valid(): bool { return $this->i < 3; }
    public function current(): mixed { return $this->i * 10; }
    public function key(): mixed { return $this->i; }
    public function next(): void { $this->i++; }
}
echo implode(",", iterator_to_array(new ItaRange())), "\n";

class ItaBag implements IteratorAggregate {
    public function getIterator(): Iterator { return new ItaRange(); }
}
echo implode(",", iterator_to_array(new ItaBag())), "\n";

echo implode(",", iterator_to_array([5, 6, 7])), "\n"; // array passthrough
?>
--EXPECT--
12
1,2
0,10,20
0,10,20
5,6,7
--CLEAN--
<?php
unset($r);
