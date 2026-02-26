--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter with a callback that matches nothing yields empty array
--FILE--
<?php
$empty = array_filter(array(1, 3, 5), function($v) { return ($v % 2) === 0; });
echo count($empty) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($empty);
