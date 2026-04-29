--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usort accepts an object with __invoke as its comparator
--FILE--
<?php
class Ascending {
    public function __invoke($a, $b) {
        return $a - $b;
    }
}
class Descending {
    public function __invoke($a, $b) {
        return $b - $a;
    }
}
$asc = [3, 1, 4, 1, 5, 9, 2, 6];
usort($asc, new Ascending());
echo implode(",", $asc), "\n";

$desc = [3, 1, 4, 1, 5, 9, 2, 6];
usort($desc, new Descending());
echo implode(",", $desc), "\n";
?>
--EXPECT--
1,1,2,3,4,5,6,9
9,6,5,4,3,2,1,1
--CLEAN--
<?php
unset($asc, $desc);
