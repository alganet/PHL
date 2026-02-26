--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with empty arrays returns empty
--FILE--
<?php
$c = array_combine(array(), array());
echo count($c) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($c);
