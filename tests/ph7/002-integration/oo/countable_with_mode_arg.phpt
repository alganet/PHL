--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with COUNT_NORMAL/COUNT_RECURSIVE mode on Countable object dispatches the same — mode is ignored
--FILE--
<?php
class CounterWithMode implements Countable {
    public function count(): int { echo "called\n"; return 7; }
}
$c = new CounterWithMode();
echo count($c), "\n";
echo count($c, COUNT_NORMAL), "\n";
echo count($c, COUNT_RECURSIVE), "\n";
?>
--EXPECT--
called
7
called
7
called
7
--CLEAN--
<?php
