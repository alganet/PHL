--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Iterator::current() inside loop body matches the foreach value
--FILE--
<?php
if (function_exists('zend_version')) error_reporting(E_ALL & ~E_DEPRECATED);
class Seq implements Iterator {
    private $i = 0;
    public function rewind() { $this->i = 0; }
    public function valid() { return $this->i < 3; }
    public function current() { return $this->i * 10; }
    public function key() { return $this->i; }
    public function next() { $this->i++; }
}
$it = new Seq();
foreach ($it as $k => $v) {
    echo "$k:$v:" . $it->current() . "\n";
}
?>
--EXPECT--
0:0:0
1:10:10
2:20:20
--CLEAN--
<?php
