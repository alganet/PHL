--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach on Iterator with early break
--FILE--
<?php
if (function_exists('zend_version')) error_reporting(E_ALL & ~E_DEPRECATED);
class Counter implements Iterator {
    private $i = 0;
    private $max;
    public function __construct($max) { $this->max = $max; }
    public function rewind() { $this->i = 0; }
    public function valid() { return $this->i < $this->max; }
    public function current() { return $this->i; }
    public function key() { return $this->i; }
    public function next() { $this->i++; }
}

foreach (new Counter(100) as $v) {
    if ($v >= 3) break;
    echo "$v\n";
}
echo "done\n";
?>
--EXPECT--
0
1
2
done
--CLEAN--
<?php
