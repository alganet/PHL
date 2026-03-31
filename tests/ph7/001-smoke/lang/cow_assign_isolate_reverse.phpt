--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: modifying original does not affect copy
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
$a[0] = 99;
echo $a[0] . "\n";
echo $b[0] . "\n";
?>
--EXPECT--
99
1
--CLEAN--
<?php
unset($a, $b);
