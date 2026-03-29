--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: in foreach
--FILE--
<?php
foreach ([10, 20, 30] as $v) {
    echo $v, "\n";
}
foreach (['a' => 1, 'b' => 2] as $k => $v) {
    echo "$k=$v\n";
}
?>
--EXPECT--
10
20
30
a=1
b=2
--CLEAN--
<?php
