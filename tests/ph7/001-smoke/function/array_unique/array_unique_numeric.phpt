--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique removes duplicate values from numeric array preserving first keys
--FILE--
<?php
$u = array_unique(array(1, 2, 2, 3, 3, 3));
foreach($u as $k=>$v) echo "$k:$v ";
echo PHP_EOL;
?>
--EXPECT--
0:1 1:2 3:3
--CLEAN--
<?php
unset($u);
