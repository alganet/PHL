--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Break in nested loops
--FILE--
<?php
$result = '';
for ($i = 0; $i < 3; $i++) {
    for ($j = 0; $j < 3; $j++) {
        if ($i == 1 && $j == 1) {
            break 2;
        }
        $result .= "$i$j ";
    }
}
$result .= "end";
echo $result;
?>
--EXPECT--
00 01 02 10 end
--CLEAN--
<?php
unset($result);
