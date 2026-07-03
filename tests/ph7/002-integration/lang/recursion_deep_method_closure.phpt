--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): instance methods, static methods, closures
--FILE--
<?php
class Counter {
    private int $hits = 0;
    public function down(int $n): int {
        $this->hits++;
        if ($n === 0) {
            return 0;
        }
        return 1 + $this->down($n - 1);
    }
    public static function sdown(int $n): int {
        return $n === 0 ? 0 : 1 + self::sdown($n - 1);
    }
    public function getHits(): int {
        return $this->hits;
    }
}
$c = new Counter();
echo $c->down(25), " ", $c->getHits(), "\n";
echo Counter::sdown(25), "\n";
$rec = function (callable $self, int $n): int {
    return $n === 0 ? 0 : 1 + $self($self, $n - 1);
};
echo $rec($rec, 25), "\n";
?>
--EXPECT--
25 26
25
25
--CLEAN--
<?php
