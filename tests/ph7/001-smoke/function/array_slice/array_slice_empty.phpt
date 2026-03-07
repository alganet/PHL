--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice on empty array returns empty array
--FILE--
<?php
$r = array_slice(array(), 0);
echo count($r) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($r);
