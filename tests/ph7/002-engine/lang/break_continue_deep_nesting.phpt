--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break and continue with deep nesting levels
--FILE--
<?php
$i = 0;
$j = 0;
$k = 0;
$l = 0;
while ($i < 2) {
    $i++;
    while ($j < 2) {
        $j++;
        while ($k < 2) {
            $k++;
            while ($l < 2) {
                $l++;
                if ($l == 1) {
                    continue 4;
                }
                echo "inner: $i,$j,$k,$l ";
            }
        }
    }
}
echo "end: $i,$j,$k,$l";
?>
--EXPECT--
inner: 2,2,2,2 end: 2,2,2,2