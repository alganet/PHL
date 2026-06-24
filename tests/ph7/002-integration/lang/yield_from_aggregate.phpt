--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: delegate over a user IteratorAggregate
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
class Seq implements Iterator {
    private $i = 0; private $d;
    function __construct(array $d) { $this->d = $d; }
    function rewind(): void { $this->i = 0; }
    function valid(): bool { return $this->i < count($this->d); }
    function current(): mixed { return array_values($this->d)[$this->i]; }
    function key(): mixed { return array_keys($this->d)[$this->i]; }
    function next(): void { $this->i++; }
}
class Bag implements IteratorAggregate {
    private $d;
    function __construct(array $d) { $this->d = $d; }
    function getIterator(): Iterator { return new Seq($this->d); }
}
function g() { yield from new Bag(["p" => 1, "q" => 2]); }
foreach (g() as $k => $v) { echo "$k=$v\n"; }
?>
--EXPECT--
p=1
q=2
--CLEAN--
<?php
