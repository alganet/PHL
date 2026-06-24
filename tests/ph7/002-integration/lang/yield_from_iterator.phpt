--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: delegate over a user Iterator
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
class Seq implements Iterator {
    private $i = 0; private $d;
    function __construct(array $d) { $this->d = $d; }
    function rewind(): void { $this->i = 0; }
    function valid(): bool { return $this->i < count($this->d); }
    function current(): mixed { return $this->d[$this->i]; }
    function key(): mixed { return $this->i; }
    function next(): void { $this->i++; }
}
function g() { yield from new Seq(["x", "y", "z"]); }
echo implode(",", iterator_to_array(g(), false)), "\n";
?>
--EXPECT--
x,y,z
--CLEAN--
<?php
