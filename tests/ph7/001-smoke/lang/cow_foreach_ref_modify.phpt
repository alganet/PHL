--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: foreach by-reference only modifies target array
--FILE--
<?php
$a = [1, 2, 3];
$b = $a;
foreach ($b as &$v) {
    $v *= 2;
}
unset($v);
echo $a[0] . " " . $a[1] . " " . $a[2] . "\n";
echo $b[0] . " " . $b[1] . " " . $b[2] . "\n";
?>
--EXPECT--
1 2 3
2 4 6
--CLEAN--
<?php
unset($a, $b);
