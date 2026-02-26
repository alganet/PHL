--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter without a callback should strip falsey values
--FILE--
<?php
$arr = array(0, 1, false, "", NULL, 2, array());
$res = array_filter($arr);
// use array_values() to ignore original keys for easier comparison
echo implode(',', array_values($res)) . PHP_EOL;
?>
--EXPECT--
1,2
--CLEAN--
<?php
unset($arr, $res);
