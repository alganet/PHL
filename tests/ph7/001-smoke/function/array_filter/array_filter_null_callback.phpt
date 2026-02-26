--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter with NULL callback should act like no callback at all
--FILE--
<?php
$arr = array(0, 1, false, "", NULL, 2, array());
$res = array_filter($arr, NULL);
echo implode(',', array_values($res)) . PHP_EOL;
?>
--EXPECT--
1,2
--CLEAN--
<?php
unset($arr, $res);
