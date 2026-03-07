--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique returns empty array for empty input
--FILE--
<?php
$u = array_unique(array());
echo count($u);
echo PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($u);
