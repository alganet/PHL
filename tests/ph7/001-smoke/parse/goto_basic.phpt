--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto basic functionality with labels
--FILE--
<?php
$i = 0;
LABEL:
echo $i . "\n";
$i++;
if ($i < 3) goto LABEL;
?>
--EXPECT--
0
1
2
--CLEAN--
<?php
unset($i);
