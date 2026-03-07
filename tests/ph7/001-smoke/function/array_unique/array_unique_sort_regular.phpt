--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique with SORT_REGULAR uses loose comparison
--FILE--
<?php
$u = array_unique(array("1", 1, true), SORT_REGULAR);
foreach($u as $k=>$v) echo "$k:" . gettype($v) . " ";
echo PHP_EOL;
?>
--EXPECT--
0:string
--CLEAN--
<?php
unset($u);
