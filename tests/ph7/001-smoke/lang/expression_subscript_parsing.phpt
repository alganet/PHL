--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expression parsing with array subscripts
--FILE--
<?php
$a = array(array(1, 2), array(3, 4));
echo $a[0][1] . "\n";
echo $a[1][0] . "\n";
?>
--EXPECT--
2
3
--CLEAN--
<?php
unset($a);
