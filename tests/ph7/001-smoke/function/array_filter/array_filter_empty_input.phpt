--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter on an empty array should return an empty array
--FILE--
<?php
$res = array_filter(array());
echo count($res) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($res);
