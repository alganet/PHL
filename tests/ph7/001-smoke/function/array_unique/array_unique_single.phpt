--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique returns single element array unchanged
--FILE--
<?php
$u = array_unique(array("x"));
foreach($u as $k=>$v) echo "$k:$v ";
echo PHP_EOL;
?>
--EXPECT--
0:x
--CLEAN--
<?php
unset($u);
