--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
IteratorAggregate support in foreach
--FILE--
<?php
if (function_exists('zend_version')) error_reporting(E_ALL & ~E_DEPRECATED);
class MyIterator implements Iterator {
    private $items;
    private $pos = 0;
    public function __construct($items) { $this->items = $items; }
    public function current() { return $this->items[$this->pos]; }
    public function key() { return $this->pos; }
    public function next() { $this->pos++; }
    public function rewind() { $this->pos = 0; }
    public function valid() { return $this->pos < count($this->items); }
}
class Collection implements IteratorAggregate {
    private $data;
    public function __construct($data) { $this->data = $data; }
    public function getIterator() { return new MyIterator($this->data); }
}
$c = new Collection(array("x", "y", "z"));
foreach($c as $k => $v) {
    echo "$k:$v\n";
}
?>
--EXPECT--
0:x
1:y
2:z
--CLEAN--
<?php
