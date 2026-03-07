--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique with all identical values keeps only first
--FILE--
<?php
$u = array_unique(array(5, 5, 5, 5));
foreach($u as $k=>$v) echo "$k:$v ";
echo PHP_EOL;
?>
--EXPECT--
0:5
--CLEAN--
<?php
unset($u);
