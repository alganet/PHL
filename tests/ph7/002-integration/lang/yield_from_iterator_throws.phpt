--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: an exception from a delegated Iterator method propagates
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
class Boom implements Iterator {
    private $i = 0;
    function rewind(): void {}
    function valid(): bool { return $this->i < 3; }
    function current(): mixed { return $this->i; }
    function key(): mixed { return $this->i; }
    function next(): void { throw new Exception("stop at " . $this->i); }
}
function g() { yield from new Boom(); }
try {
    foreach (g() as $v) { echo "got $v\n"; }
} catch (\Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
got 0
caught: stop at 0
--CLEAN--
<?php
