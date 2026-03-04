--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill should treat boolean count as integer (true->1, false->0)
--FILE--
<?php
echo count(array_fill(0, true, 'x')) . "\n";
echo count(array_fill(0, false, 'x')) . "\n";
?>
--EXPECT--
1
0
--CLEAN--
<?php

