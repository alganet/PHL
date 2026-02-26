--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter callback return values must be converted to boolean
--FILE--
<?php
$arr = array(1,2,3);
// callback returns v-2 which yields -1,0,1 -> bool true,false,true
$out = array_filter($arr, function($v){ return $v - 2; });
echo implode(',', array_keys($out)) . PHP_EOL;
?>
--EXPECT--
0,2
--CLEAN--
<?php
unset($arr, $out);
