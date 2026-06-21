--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception thrown by an iterator method (or apply callback) propagates and is catchable
--FILE--
<?php
class ItaThrower implements Iterator {
    public function rewind(): void {}
    public function valid(): bool { return true; }
    public function current(): mixed { throw new RuntimeException("from current"); }
    public function key(): mixed { return 0; }
    public function next(): void {}
}
try { iterator_to_array(new ItaThrower()); }
catch (RuntimeException $e) { echo "caught: ", $e->getMessage(), "\n"; }

function itaOne() { yield 1; }
try { iterator_apply(itaOne(), function () { throw new RuntimeException("from callback"); }); }
catch (RuntimeException $e) { echo "caught: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
caught: from current
caught: from callback
--CLEAN--
<?php
