--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: zero count returns empty array
--FILE--
<?php
$a = array_fill(5, 0, 'y');
echo count($a) . "\n";
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a);
