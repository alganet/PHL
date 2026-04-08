--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested list() unpacking in foreach
--FILE--
<?php
$arr = array(array(array(1, 2), 3), array(array(4, 5), 6));
foreach ($arr as list(list($a, $b), $c)) {
    echo "$a-$b-$c\n";
}
?>
--EXPECT--
1-2-3
4-5-6
--CLEAN--
<?php
