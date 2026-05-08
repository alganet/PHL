--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() dispatches to ->count() for objects implementing Countable
--FILE--
<?php
class Counted implements Countable {
    private int $n;
    public function __construct(int $n) { $this->n = $n; }
    public function count(): int {
        echo "count called\n";
        return $this->n;
    }
}
echo count(new Counted(0)), "\n";
echo count(new Counted(7)), "\n";
echo count(new Counted(123)), "\n";
?>
--EXPECT--
count called
0
count called
7
count called
123
--CLEAN--
<?php
