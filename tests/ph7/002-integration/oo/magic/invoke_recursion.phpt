--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke supports self-recursion via $this()
--FILE--
<?php
class Sumdown {
    public function __invoke($n) {
        if ($n <= 0) {
            return 0;
        }
        return $n + ($this)($n - 1);
    }
}
$s = new Sumdown();
echo $s(5), "\n";
echo $s(10), "\n";
echo $s(0), "\n";
?>
--EXPECT--
15
55
0
--CLEAN--
<?php
unset($s);
