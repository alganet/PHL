--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique removes duplicate string values
--FILE--
<?php
$u = array_unique(array("a", "b", "a", "c"));
foreach($u as $k=>$v) echo "$k:$v ";
echo PHP_EOL;
?>
--EXPECT--
0:a 1:b 3:c
--CLEAN--
<?php
unset($u);
