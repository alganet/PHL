--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__invoke resolves named arguments
--FILE--
<?php
class Pair {
    public function __invoke($x, $y) {
        return "$x:$y";
    }
}
$p = new Pair();
echo $p(y: "Y", x: "X"), "\n";
echo $p(x: 1, y: 2), "\n";
echo $p(1, y: 2), "\n";
?>
--EXPECT--
X:Y
1:2
1:2
--CLEAN--
<?php
unset($p);
