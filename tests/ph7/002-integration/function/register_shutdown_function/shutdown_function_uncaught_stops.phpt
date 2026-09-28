--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwable a shutdown callback never catches skips the callbacks after it
--DESCRIPTION--
php's shutdown callbacks run under one bailout guard: the first uncaught
throwable leaves the loop, so every callback registered after it is skipped --
the same rule `exit()` inside one already followed. The destructor phase that
comes next still runs, which is why the global object is destructed after the
fatal.

The trace line is wildcarded: a frame the ENGINE pushed (nothing in the source
called this closure) is php's `[internal function]`, which PHL does not report
that way yet.
--FILE--
<?php
class Mark { public function __destruct() { echo "D:global\n"; } }

$m = new Mark();
register_shutdown_function(function () {
    echo "first\n";
    throw new LogicException('stop');
});
register_shutdown_function(function () { echo "second\n"; });
echo "script-end\n";
?>
--EXPECT--
script-end
first
D:global
--EXPECT_STDERR--
PHP Fatal error:  Uncaught LogicException: stop in %s:7
Stack trace:
#0 %A: {closure:%s:5}()
#1 {main}
  thrown in %s on line 7
