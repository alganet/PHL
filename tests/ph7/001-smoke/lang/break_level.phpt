--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Break with numeric level in nested loops
--FILE--
<?php
$result = '';
for ($i = 0; $i < 3; $i++) {
    $result .= "i$i ";
    for ($j = 0; $j < 3; $j++) {
        $result .= "j$j ";
        if ($i == 1 && $j == 1) {
            break 2; // break out of both loops
        }
    }
}
echo $result;
?>
--EXPECT--
i0 j0 j1 j2 i1 j0 j1
--CLEAN--
<?php
unset($result);
