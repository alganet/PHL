--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse on an empty array returns an empty array
--FILE--
<?php
$r = array_reverse(array());
echo count($r) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($r);
