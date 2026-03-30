--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach on object implementing Iterator
--FILE--
<?php
if (function_exists('zend_version')) error_reporting(E_ALL & ~E_DEPRECATED);
class Range implements Iterator {
    private $current;
    private $start;
    private $end;
    public function __construct($start, $end) {
        $this->start = $start;
        $this->end = $end;
    }
    public function rewind() { $this->current = $this->start; }
    public function valid() { return $this->current <= $this->end; }
    public function current() { return $this->current; }
    public function key() { return $this->current - $this->start; }
    public function next() { $this->current++; }
}

foreach (new Range(1, 5) as $k => $v) {
    echo "$k: $v\n";
}
?>
--EXPECT--
0: 1
1: 2
2: 3
3: 4
4: 5
--CLEAN--
<?php
