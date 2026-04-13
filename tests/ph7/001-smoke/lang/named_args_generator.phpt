--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: generator functions with out-of-order args
--FILE--
<?php
function nagenf($start, $end) {
    for ($i = $start; $i <= $end; $i++) {
        yield $i;
    }
}
$result = [];
foreach (nagenf(end: 5, start: 3) as $v) {
    $result[] = $v;
}
echo implode(",", $result) . "\n";

$result2 = [];
foreach (nagenf(start: 10, end: 12) as $v) {
    $result2[] = $v;
}
echo implode(",", $result2) . "\n";
?>
--EXPECT--
3,4,5
10,11,12
--CLEAN--
<?php
