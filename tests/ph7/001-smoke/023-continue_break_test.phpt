--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Continue and break statements
--FILE--
<?php
$sum = 0;
for ($i = 0; $i < 10; $i++) {
    if ($i % 2 == 0) {
        continue; // skip even numbers
    }
    if ($i > 6) {
        break; // stop at 7
    }
    $sum += $i;
}
echo $sum; // 1+3+5 = 9
?>
--EXPECT--
9
--CLEAN--
<?php
unset($sum, $i);
?>