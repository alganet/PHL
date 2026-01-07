--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with complex control flow
--FILE--
<?php
$i = 0;
start:
if ($i < 3) {
    echo $i . " ";
    $i++;
    goto start;
}
echo "done";
?>
--EXPECT--
0 1 2 done