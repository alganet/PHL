--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: basic usage
--FILE--
<?php
$a = [1, 2, 3];
echo $a[0], "\n";
echo $a[1], "\n";
echo $a[2], "\n";
echo count($a), "\n";
?>
--EXPECT--
1
2
3
3
--CLEAN--
<?php
unset($a);
