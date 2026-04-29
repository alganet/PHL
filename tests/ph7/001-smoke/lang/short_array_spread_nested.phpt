--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread inside nested array literals (non-recursive)
--FILE--
<?php
$a = [[1, ...[2, 3]], [...["a" => 4]]];
echo count($a), "\n";
echo count($a[0]), "\n";
foreach ($a[0] as $k => $v) {
    echo "0:", $k, "=", $v, "\n";
}
echo count($a[1]), "\n";
foreach ($a[1] as $k => $v) {
    echo "1:", $k, "=", $v, "\n";
}

$inner = [10, 20];
$nest = [[...$inner, 30], [...$inner]];
foreach ($nest as $i => $row) {
    foreach ($row as $k => $v) {
        echo $i, ":", $k, "=", $v, "\n";
    }
}
?>
--EXPECT--
2
3
0:0=1
0:1=2
0:2=3
1
1:a=4
0:0=10
0:1=20
0:2=30
1:0=10
1:1=20
--CLEAN--
<?php
unset($a, $inner, $nest);
