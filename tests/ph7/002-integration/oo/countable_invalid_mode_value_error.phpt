--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() validates $mode before dispatching to Countable::count(): an invalid mode raises ValueError and the method is never called
--FILE--
<?php
class LoudCounter implements Countable {
    public function count(): int { echo "count() called\n"; return 7; }
}
$c = new LoudCounter();
try {
    count($c, 99);
    echo "no error\n";
} catch (ValueError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE
--CLEAN--
<?php
unset($c);
