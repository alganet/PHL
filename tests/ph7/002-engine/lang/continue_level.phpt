--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Continue with numeric level in nested loops
--FILE--
<?php
$result = '';
for ($i = 0; $i < 3; $i++) {
    $result .= "i$i ";
    for ($j = 0; $j < 3; $j++) {
        if ($i == 1 && $j == 0) {
            continue 2; // skip to next i
        }
        $result .= "j$j ";
    }
}
echo $result;
?>
--EXPECT--
i0 j0 j1 j2 i1 i2 j0 j1 j2
--CLEAN--
<?php
unset($result, $i, $j);
?>