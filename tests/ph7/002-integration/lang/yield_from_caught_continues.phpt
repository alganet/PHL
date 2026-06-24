--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: a generator-internal exception caught by the caller resumes after the catch
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
// Regression: the throw-unwind routes to the generator's terminal OP_DONE with
// an empty operand stack; the result-store branch used to read below the stack
// base, corrupting VM state (segfault under glibc/ASan, dropped continuation
// elsewhere). The statement after a caller-side catch must run.

// non-iterable yield from -> Error, caught by the caller, then continue
function g() { yield from 5; }
try {
    foreach (g() as $v) {}
} catch (\Error $e) {
    echo "caught ", get_class($e), "\n";
}
echo "after-typeerror\n";

// a delegated Iterator method throws, caught by the caller, then continue
class Boom implements Iterator {
    private $i = 0;
    function rewind(): void {}
    function valid(): bool { return $this->i < 3; }
    function current(): mixed { return $this->i; }
    function key(): mixed { return $this->i; }
    function next(): void { throw new Exception("stop at " . $this->i); }
}
function gi() { yield from new Boom(); }
try {
    foreach (gi() as $v) { echo "got $v\n"; }
} catch (\Throwable $e) {
    echo "caught ", $e->getMessage(), "\n";
}
echo "after-iterator\n";
?>
--EXPECT--
caught Error
after-typeerror
got 0
caught stop at 0
after-iterator
--CLEAN--
<?php
